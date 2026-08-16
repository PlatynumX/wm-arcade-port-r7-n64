#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wm/anim.h"
#include "wm/demo.h"
#include "wm/game.h"
#include "wm/source_data.h"
#include "wm/bret_visuals.h"
#include "wm/visual.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        exit(1); \
    } \
} while (0)

static int runs;
static void sleeper(wm_process *p, void *user) {
    wm_scheduler *s = (wm_scheduler *)user;
    ++runs;
    if (runs == 1) wm_process_sleep(s, p, 2);
    else wm_process_kill(p);
}

static void test_scheduler(void) {
    wm_scheduler s;
    runs = 0;
    wm_scheduler_init(&s);
    CHECK(wm_process_create(&s, 1, sleeper, &s) != NULL);
    wm_scheduler_step(&s);
    wm_scheduler_step(&s);
    wm_scheduler_step(&s);
    CHECK(runs == 2);
}

static void test_source_sequence(void) {
    const uint16_t expected[] = {
        0x8002, 0x000c, 0x8003, 0x8026, 0x0100,
        0x801b, 0x805f, 0x8002, 0x0000, 0x8049
    };
    CHECK(wm_source_hrt_finish1_move.word_count == sizeof(expected)/sizeof(expected[0]));
    for (size_t i = 0; i < sizeof(expected)/sizeof(expected[0]); ++i)
        CHECK(wm_source_hrt_finish1_move.words[i] == expected[i]);
}

static void test_anim(void) {
    wm_object o = {.vx=1,.vy=2,.vz=3,.visible=true};
    wm_anim_state a;
    wm_anim_start(&a, wm_source_hrt_finish1_move.words,
                  wm_source_hrt_finish1_move.word_count);
    CHECK(!wm_anim_step(&a, &o));
    CHECK(a.ended);
    CHECK(o.vx == 0 && o.vy == 0 && o.vz == 0);
    CHECK(a.speed == 0x100);
    CHECK((a.mode & WM_MODE_END) != 0);
}

static void test_ropes(void) {
    wm_rope_system r;
    wm_ropes_init(&r);
    CHECK(wm_rope_command(&r, WM_ROPE_LEFT, WM_ROPE_CMD_2, 3, wm_fix_from_int(42)));
    CHECK(r.group[WM_ROPE_LEFT].generation == 1);
    CHECK(r.group[WM_ROPE_LEFT].position_or_magnitude == 3);
    CHECK(wm_fix_to_int(r.group[WM_ROPE_LEFT].wrestler_z) == 42);
}

static void test_visual_sequences(void) {
    CHECK(wm_bret_stand2_anim.frame_count == 14);
    CHECK(wm_bret_stand4_anim.frame_count == 14);
    CHECK(wm_bret_torso2_anim.frame_count == 6 && wm_bret_torso2_anim.repeat);
    CHECK(wm_bret_torso4_anim.frame_count == 6 && wm_bret_torso4_anim.repeat);
    CHECK(wm_bret_walk2_f2_anim.frame_count == 16);
    CHECK(wm_bret_walk8_f2_anim.frame_count == 16);
    CHECK(wm_bret_walk4_f4_anim.frame_count == 16);
    CHECK(wm_bret_walk6_f4_anim.frame_count == 15);
    CHECK(wm_bret_run_anim.frame_count == 12 && wm_bret_run_anim.repeat);
    CHECK(wm_bret_punch2_anim.frame_count == 11 && !wm_bret_punch2_anim.repeat);
    CHECK(wm_bret_punch4_anim.frame_count == 11 && !wm_bret_punch4_anim.repeat);
    CHECK(wm_bret_kick2_anim.frame_count == 12 && !wm_bret_kick2_anim.repeat);
    CHECK(wm_bret_kick4_anim.frame_count == 12 && !wm_bret_kick4_anim.repeat);

    wm_visual_state v;
    wm_visual_start(&v, &wm_bret_stand4_anim);
    CHECK(wm_visual_current(&v) != NULL);
    for (int i = 0; i < 4; ++i) wm_visual_tick(&v);
    CHECK(v.frame_index == 0);
    wm_visual_tick(&v);
    CHECK(v.frame_index == 1);
}

static void test_demo_four_way_and_run(void) {
    wm_demo d;
    wm_demo_init(&d);
    d.ai_enabled = false;
    CHECK(d.p1.action == WM_DEMO_IDLE);
    CHECK(d.p1.facing == WM_DEMO_FACING_6);
    CHECK(d.p1.visual.sequence == &wm_bret_stand4_anim);
    CHECK(d.p1.torso_visual.sequence == &wm_bret_torso4_anim);

    wm_input_state right = {.stick_x = 80};
    wm_demo_tick(&d, &right);
    CHECK(d.p1.action == WM_DEMO_WALK);
    CHECK(d.p1.facing == WM_DEMO_FACING_6);
    CHECK(d.p1.flip_x);
    CHECK(d.p1.visual.sequence == &wm_bret_walk6_f4_anim);
    CHECK(d.p1.torso_visual.sequence == &wm_bret_torso4_anim);

    wm_input_state left = {.stick_x = -80};
    wm_demo_tick(&d, &left);
    CHECK(d.p1.facing == WM_DEMO_FACING_4);
    CHECK(!d.p1.flip_x);
    CHECK(d.p1.visual.sequence == &wm_bret_walk4_f4_anim);

    wm_input_state up_run = {.stick_y = 80, .z = true};
    int y0 = d.p1.screen_y;
    wm_demo_tick(&d, &up_run);
    CHECK(d.p1.action == WM_DEMO_RUN);
    CHECK(d.p1.facing == WM_DEMO_FACING_8);
    CHECK(d.p1.screen_y < y0);
    CHECK(d.p1.visual.sequence == &wm_bret_run_anim);
    CHECK(d.p1.torso_visual.sequence == &wm_bret_torso2_anim);

    wm_input_state down = {.stick_y = -80};
    wm_demo_tick(&d, &down);
    CHECK(d.p1.action == WM_DEMO_WALK);
    CHECK(d.p1.facing == WM_DEMO_FACING_2);
    CHECK(d.p1.visual.sequence == &wm_bret_walk2_f2_anim);
}

static void tick_until_player_action_done(wm_demo *d) {
    wm_input_state none = {0};
    for (int i = 0; i < 128 &&
         (d->p1.action == WM_DEMO_PUNCH || d->p1.action == WM_DEMO_KICK); ++i)
        wm_demo_tick(d, &none);
}

static void test_demo_attacks_and_hits(void) {
    wm_demo d;
    wm_demo_init(&d);
    d.ai_enabled = false;
    d.p1.screen_x = 120;
    d.p1.screen_y = 170;
    d.p1.facing = WM_DEMO_FACING_6;
    d.p2.screen_x = 156;
    d.p2.screen_y = 170;
    d.p2.facing = WM_DEMO_FACING_4;

    wm_input_state punch = {.a = true};
    wm_demo_tick(&d, &punch);
    CHECK(d.p1.action == WM_DEMO_PUNCH);
    CHECK(d.p1.visual.sequence == &wm_bret_punch4_anim);
    CHECK(d.p1.action_count == 1);

    int old_hp = d.p2.health;
    wm_input_state none = {0};
    for (int i = 0; i < 32 && d.p2.health == old_hp; ++i)
        wm_demo_tick(&d, &none);
    CHECK(d.p2.health == old_hp - 8);
    CHECK(d.p1.hit_count == 1);
    CHECK(d.total_hits == 1);
    tick_until_player_action_done(&d);
    CHECK(d.p1.action == WM_DEMO_IDLE);

    /* One attack may only score once even while several source frames are active. */
    CHECK(d.p2.health == old_hp - 8);

    d.p2.stun_ticks = 0;
    d.p1.facing = WM_DEMO_FACING_6;
    d.p2.screen_x = d.p1.screen_x + 40;
    d.p2.screen_y = d.p1.screen_y;
    wm_input_state kick = {.b = true};
    wm_demo_tick(&d, &kick);
    int hp2 = d.p2.health;
    for (int i = 0; i < 40 && d.p2.health == hp2; ++i)
        wm_demo_tick(&d, &none);
    CHECK(d.p2.health == hp2 - 12);
    CHECK(d.p1.hit_count == 2);
}

static void test_cpu_chases(void) {
    wm_demo d;
    wm_demo_init(&d);
    d.p1.screen_x = 80;
    d.p1.screen_y = 170;
    d.p2.screen_x = 240;
    d.p2.screen_y = 170;
    d.ai_enabled = true;
    int x0 = d.p2.screen_x;
    wm_input_state none = {0};
    for (int i = 0; i < 8; ++i)
        wm_demo_tick(&d, &none);
    CHECK(d.p2.screen_x < x0);
    CHECK(d.p2.action == WM_DEMO_RUN || d.p2.action == WM_DEMO_WALK);
}

static void test_demo_bounds(void) {
    wm_demo d;
    wm_demo_init(&d);
    d.ai_enabled = false;
    wm_input_state up = {.stick_y = 100};
    wm_input_state left = {.stick_x = -100};
    for (int i = 0; i < 200; ++i) wm_demo_tick(&d, &up);
    CHECK(d.p1.screen_y >= 142);
    for (int i = 0; i < 200; ++i) wm_demo_tick(&d, &left);
    CHECK(d.p1.screen_x >= 68);
}

static void test_demo_vm_replay(void) {
    wm_demo d;
    wm_demo_init(&d);
    d.ai_enabled = false;
    wm_input_state none = {0};
    wm_demo_tick(&d, &none);
    CHECK(d.anim.ended);
    unsigned restarts = d.restarts;
    wm_input_state replay = {.c_up = true};
    wm_demo_tick(&d, &replay);
    CHECK(d.restarts == restarts + 1);
    CHECK(d.anim.ended);
}

int main(void) {
    test_scheduler();
    test_source_sequence();
    test_anim();
    test_ropes();
    test_visual_sequences();
    test_demo_four_way_and_run();
    test_demo_attacks_and_hits();
    test_cpu_chases();
    test_demo_bounds();
    test_demo_vm_replay();
    puts("all core tests passed");
    return 0;
}
