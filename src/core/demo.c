#include "wm/demo.h"
#include "wm/bret_visuals.h"
#include "wm/source_data.h"
#include <stdlib.h>
#include <string.h>

#define MOVE_DEADZONE 12
/* These are anchor bounds, tightened after the r5 hardware video showed the
   source registration point could move far enough that most of Bret left the
   visible ring. */
#define RING_X_MIN 68
#define RING_X_MAX 252
#define RING_Y_MIN 142
#define RING_Y_MAX 190
#define MAX_HEALTH 100

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int sign_int(int v) { return (v > 0) - (v < 0); }

static void restart_source_anim(wm_demo *d) {
    d->wrestler.vx = 0;
    d->wrestler.vy = 0;
    d->wrestler.vz = 0;
    d->wrestler.visible = true;
    wm_anim_start(&d->anim,
                  wm_source_hrt_finish1_move.words,
                  wm_source_hrt_finish1_move.word_count);
    ++d->restarts;
}

static bool horizontal_facing(wm_demo_facing facing) {
    return facing == WM_DEMO_FACING_4 || facing == WM_DEMO_FACING_6;
}

static const wm_visual_sequence *sequence_for(const wm_demo_fighter *f) {
    switch (f->action) {
        case WM_DEMO_PUNCH:
            return horizontal_facing(f->facing) ? &wm_bret_punch4_anim : &wm_bret_punch2_anim;
        case WM_DEMO_KICK:
            return horizontal_facing(f->facing) ? &wm_bret_kick4_anim : &wm_bret_kick2_anim;
        case WM_DEMO_RUN:
            return &wm_bret_run_anim;
        case WM_DEMO_WALK:
            switch (f->facing) {
                case WM_DEMO_FACING_2: return &wm_bret_walk2_f2_anim;
                case WM_DEMO_FACING_8: return &wm_bret_walk8_f2_anim;
                case WM_DEMO_FACING_4: return &wm_bret_walk4_f4_anim;
                case WM_DEMO_FACING_6: return &wm_bret_walk6_f4_anim;
            }
            break;
        case WM_DEMO_IDLE:
        default:
            return horizontal_facing(f->facing) ? &wm_bret_stand4_anim : &wm_bret_stand2_anim;
    }
    return &wm_bret_stand2_anim;
}

static const wm_visual_sequence *torso_sequence_for(const wm_demo_fighter *f) {
    return horizontal_facing(f->facing) ? &wm_bret_torso4_anim : &wm_bret_torso2_anim;
}

static void refresh_flip(wm_demo_fighter *f) {
    f->flip_x = f->facing == WM_DEMO_FACING_6;
}

static void set_action(wm_demo_fighter *f, wm_demo_action action) {
    const wm_visual_sequence *next;
    const wm_visual_sequence *torso_next;
    f->action = action;
    refresh_flip(f);
    next = sequence_for(f);
    if (f->visual.sequence != next || f->visual.ended) {
        wm_visual_start(&f->visual, next);
        if (action == WM_DEMO_PUNCH || action == WM_DEMO_KICK)
            f->attack_connected = false;
    }

    /*
     * Original WrestleMania is not a one-sprite wrestler here. Bret's init
     * starts animation channel 1 with the stance and channel 2 with an
     * independent H2TW2A/H4TW4A torso stream. Keep that second stream alive
     * and direction-correct even while actions temporarily hide it.
     */
    torso_next = torso_sequence_for(f);
    if (f->torso_visual.sequence != torso_next || f->torso_visual.ended)
        wm_visual_start(&f->torso_visual, torso_next);
}

static bool is_attack(const wm_demo_fighter *f) {
    return f->action == WM_DEMO_PUNCH || f->action == WM_DEMO_KICK;
}

static bool is_moving(const wm_input_state *input) {
    return input && (abs((int)input->stick_x) > MOVE_DEADZONE ||
                     abs((int)input->stick_y) > MOVE_DEADZONE);
}

static void update_facing_from_vector(wm_demo_fighter *f, int x, int y) {
    if (abs(x) > abs(y))
        f->facing = x < 0 ? WM_DEMO_FACING_4 : WM_DEMO_FACING_6;
    else if (y != 0)
        f->facing = y > 0 ? WM_DEMO_FACING_2 : WM_DEMO_FACING_8;
    refresh_flip(f);
}

static void update_facing_from_stick(wm_demo_fighter *f, const wm_input_state *input) {
    /* Screen Y is inverted relative to N64 stick Y. */
    update_facing_from_vector(f, input->stick_x, -input->stick_y);
}

static void move_fighter(wm_demo_fighter *f, int stick_x, int stick_y, bool running) {
    int dx = stick_x / (running ? 16 : 24);
    int dy = -stick_y / (running ? 16 : 24);

    if (dx == 0 && abs(stick_x) > MOVE_DEADZONE)
        dx = stick_x < 0 ? -1 : 1;
    if (dy == 0 && abs(stick_y) > MOVE_DEADZONE)
        dy = stick_y < 0 ? 1 : -1;

    f->screen_x = clamp_int(f->screen_x + dx, RING_X_MIN, RING_X_MAX);
    f->screen_y = clamp_int(f->screen_y + dy, RING_Y_MIN, RING_Y_MAX);
}

static bool attack_active(const wm_demo_fighter *f) {
    if (!is_attack(f) || f->visual.ended)
        return false;

    /* HRTSEQ2.ASM turns ANI_ATTACK_ON immediately before the first FR4 of
       Bret's normal punch and the first FR5 of his normal kick, then turns it
       off immediately afterward.  In the flattened visual tables those are
       both frame_index 5.  The later duplicate FR4/FR5 recovery frames are
       deliberately *not* active. */
    return f->visual.frame_index == 5;
}

static bool in_attack_range(const wm_demo_fighter *attacker,
                            const wm_demo_fighter *victim) {
    const int dx = victim->screen_x - attacker->screen_x;
    const int dy = victim->screen_y - attacker->screen_y;
    int forward, lateral;

    /* Normal punch source box: x=30,w=50,d=45.
       Normal kick source box:  x=23,w=50,h=17.
       The portable 2-D ring plane uses the original forward reach and a
       conservative lateral projection while the full 3-D attack-box transform
       is still being translated.  Victim radii allow center-point overlap. */
    const int box_x = attacker->action == WM_DEMO_KICK ? 23 : 30;
    const int box_w = 50;
    const int victim_forward_radius = 18;
    const int lateral_radius = attacker->action == WM_DEMO_KICK ? 20 : 32;

    switch (attacker->facing) {
        case WM_DEMO_FACING_4: forward = -dx; lateral = dy; break;
        case WM_DEMO_FACING_6: forward =  dx; lateral = dy; break;
        case WM_DEMO_FACING_8: forward = -dy; lateral = dx; break;
        case WM_DEMO_FACING_2: forward =  dy; lateral = dx; break;
        default: return false;
    }

    return forward >= box_x - victim_forward_radius &&
           forward <= box_x + box_w + victim_forward_radius &&
           abs(lateral) <= lateral_radius;
}

static void apply_hit(wm_demo *d, wm_demo_fighter *attacker,
                      wm_demo_fighter *victim) {
    int dx, dy;
    const int damage = attacker->action == WM_DEMO_KICK ? 12 : 8;
    const unsigned stun = attacker->action == WM_DEMO_KICK ? 14u : 10u;

    attacker->attack_connected = true;
    ++attacker->hit_count;
    ++d->total_hits;
    victim->health -= damage;
    if (victim->health < 0) victim->health = 0;
    victim->stun_ticks = stun;

    dx = sign_int(victim->screen_x - attacker->screen_x);
    dy = sign_int(victim->screen_y - attacker->screen_y);
    if (dx == 0 && dy == 0) {
        if (horizontal_facing(attacker->facing))
            dx = attacker->facing == WM_DEMO_FACING_4 ? -1 : 1;
        else
            dy = attacker->facing == WM_DEMO_FACING_8 ? -1 : 1;
    }
    victim->screen_x = clamp_int(victim->screen_x + dx * 9, RING_X_MIN, RING_X_MAX);
    victim->screen_y = clamp_int(victim->screen_y + dy * 5, RING_Y_MIN, RING_Y_MAX);
    set_action(victim, WM_DEMO_IDLE);
}

static void resolve_attack(wm_demo *d, wm_demo_fighter *attacker,
                           wm_demo_fighter *victim) {
    if (attacker->attack_connected || !attack_active(attacker) || victim->health <= 0)
        return;
    if (in_attack_range(attacker, victim))
        apply_hit(d, attacker, victim);
}

static void tick_one_shot(wm_demo_fighter *f) {
    wm_visual_tick(&f->visual);
    if (f->visual.ended)
        set_action(f, WM_DEMO_IDLE);
}

static void keep_fighters_separated(wm_demo_fighter *a, wm_demo_fighter *b) {
    int dx = b->screen_x - a->screen_x;
    int dy = b->screen_y - a->screen_y;
    const int min_dx = 26;
    const int min_dy = 13;
    if (abs(dx) >= min_dx || abs(dy) >= min_dy)
        return;

    /* r6h1 could leave the two identical Brets nearly perfectly superimposed,
       which looked like one wrestler disappearing. Resolve the overlap in a
       single tick instead of nudging one pixel per frame. */
    if (abs(dx) >= abs(dy)) {
        int dir = dx >= 0 ? 1 : -1;
        int need = min_dx - abs(dx);
        int push_a = (need + 1) / 2;
        int push_b = need / 2;
        a->screen_x = clamp_int(a->screen_x - dir * push_a, RING_X_MIN, RING_X_MAX);
        b->screen_x = clamp_int(b->screen_x + dir * push_b, RING_X_MIN, RING_X_MAX);
    } else {
        int dir = dy >= 0 ? 1 : -1;
        int need = min_dy - abs(dy);
        int push_a = (need + 1) / 2;
        int push_b = need / 2;
        a->screen_y = clamp_int(a->screen_y - dir * push_a, RING_Y_MIN, RING_Y_MAX);
        b->screen_y = clamp_int(b->screen_y + dir * push_b, RING_Y_MIN, RING_Y_MAX);
    }
}

static void tick_player(wm_demo *d, const wm_input_state *input) {
    wm_demo_fighter *p = &d->p1;
    bool moving;

    if (p->health <= 0) {
        set_action(p, WM_DEMO_IDLE);
        wm_visual_tick(&p->visual);
        return;
    }
    if (p->stun_ticks) {
        --p->stun_ticks;
        set_action(p, WM_DEMO_IDLE);
        wm_visual_tick(&p->visual);
        return;
    }
    if (is_attack(p)) {
        if (!p->visual.ended) {
            tick_one_shot(p);
            return;
        }
        set_action(p, WM_DEMO_IDLE);
    }

    moving = is_moving(input);
    if (moving)
        update_facing_from_stick(p, input);

    if (input && input->a) {
        set_action(p, WM_DEMO_PUNCH);
        ++p->action_count;
    } else if (input && input->b) {
        set_action(p, WM_DEMO_KICK);
        ++p->action_count;
    } else if (moving) {
        const bool running = input && input->z;
        set_action(p, running ? WM_DEMO_RUN : WM_DEMO_WALK);
        move_fighter(p, input->stick_x, input->stick_y, running);
    } else {
        set_action(p, WM_DEMO_IDLE);
    }
    wm_visual_tick(&p->visual);
}

static void tick_cpu(wm_demo *d) {
    wm_demo_fighter *cpu = &d->p2;
    wm_demo_fighter *target = &d->p1;
    int dx = target->screen_x - cpu->screen_x;
    int dy = target->screen_y - cpu->screen_y;

    if (cpu->health <= 0) {
        set_action(cpu, WM_DEMO_IDLE);
        wm_visual_tick(&cpu->visual);
        return;
    }
    if (cpu->stun_ticks) {
        --cpu->stun_ticks;
        set_action(cpu, WM_DEMO_IDLE);
        wm_visual_tick(&cpu->visual);
        return;
    }
    if (is_attack(cpu)) {
        if (!cpu->visual.ended) {
            tick_one_shot(cpu);
            return;
        }
        set_action(cpu, WM_DEMO_IDLE);
    }
    if (!d->ai_enabled) {
        set_action(cpu, WM_DEMO_IDLE);
        wm_visual_tick(&cpu->visual);
        return;
    }

    update_facing_from_vector(cpu, dx, dy);
    if (d->ai_cooldown)
        --d->ai_cooldown;

    if (abs(dx) <= 42 && abs(dy) <= 22 && d->ai_cooldown == 0) {
        set_action(cpu, ((d->game.frame >> 4) & 1) ? WM_DEMO_KICK : WM_DEMO_PUNCH);
        ++cpu->action_count;
        d->ai_cooldown = 32;
    } else {
        int sx = clamp_int(dx * 3, -90, 90);
        int sy = clamp_int(-dy * 3, -90, 90);
        const bool run = abs(dx) + abs(dy) > 105;
        set_action(cpu, run ? WM_DEMO_RUN : WM_DEMO_WALK);
        move_fighter(cpu, sx, sy, run);
    }
    wm_visual_tick(&cpu->visual);
}

void wm_demo_reset_match(wm_demo *d) {
    memset(&d->p1, 0, sizeof(d->p1));
    memset(&d->p2, 0, sizeof(d->p2));

    d->p1.screen_x = 118;
    d->p1.screen_y = 172;
    d->p1.facing = WM_DEMO_FACING_6;
    d->p1.health = MAX_HEALTH;
    set_action(&d->p1, WM_DEMO_IDLE);

    d->p2.screen_x = 208;
    d->p2.screen_y = 162;
    d->p2.facing = WM_DEMO_FACING_4;
    d->p2.health = MAX_HEALTH;
    set_action(&d->p2, WM_DEMO_IDLE);

    d->ai_enabled = true;
    d->ai_cooldown = 30;
    d->total_hits = 0;
}

void wm_demo_init(wm_demo *d) {
    memset(d, 0, sizeof(*d));
    wm_game_init(&d->game);
    d->wrestler.visible = true;
    wm_demo_reset_match(d);
    restart_source_anim(d);
}

void wm_demo_tick(wm_demo *d, const wm_input_state *input) {
    if (input && input->l)
        d->ai_enabled = !d->ai_enabled;
    if (input && input->r)
        wm_demo_reset_match(d);

    tick_player(d, input);
    tick_cpu(d);

    /* The arcade advances the second torso animation independently of the
       primary leg/body stream. */
    wm_visual_tick(&d->p1.torso_visual);
    wm_visual_tick(&d->p2.torso_visual);

    keep_fighters_separated(&d->p1, &d->p2);

    resolve_attack(d, &d->p1, &d->p2);
    resolve_attack(d, &d->p2, &d->p1);

    if (input && input->c_up && d->anim.ended)
        restart_source_anim(d);

    if (!d->anim.ended)
        (void)wm_anim_step(&d->anim, &d->wrestler);
    wm_game_tick(&d->game);
}

const char *wm_demo_action_name(wm_demo_action action) {
    switch (action) {
        case WM_DEMO_IDLE: return "IDLE";
        case WM_DEMO_WALK: return "WALK";
        case WM_DEMO_RUN: return "RUN";
        case WM_DEMO_PUNCH: return "PUNCH";
        case WM_DEMO_KICK: return "KICK";
    }
    return "?";
}

const char *wm_demo_facing_name(wm_demo_facing facing) {
    switch (facing) {
        case WM_DEMO_FACING_2: return "2/DOWN";
        case WM_DEMO_FACING_4: return "4/LEFT";
        case WM_DEMO_FACING_6: return "6/RIGHT";
        case WM_DEMO_FACING_8: return "8/UP";
    }
    return "?";
}
