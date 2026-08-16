#include <libdragon.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "wm/bret_sprites.h"
#include "wm/demo.h"
#include "wm/source_data.h"
#include "wm/visual.h"

#define STICK_DEADZONE 12

/* r7: r6h4 hardware scanning established the channel-2 attachment pair at
   raw WIMP tail words 3/4 (entry offsets +38/+40), positive delta. */
#define WM_ATTACH_X_SLOT 3
#define WM_ATTACH_Y_SLOT 4

/* Hardware input diagnostics.  The portable core already passes B->kick tests,
   so keep one build worth of raw edge counters in the N64 frontend. */
static joypad_buttons_t g_prev_buttons;
static unsigned g_pad_a_edges;
static unsigned g_pad_b_edges;

static wm_input_state read_input(bool *connected) {
    wm_input_state out = {0};
    joypad_poll();
    *connected = joypad_is_connected(JOYPAD_PORT_1);
    if (!*connected) {
        g_prev_buttons.raw = 0;
        return out;
    }

    const joypad_inputs_t in = joypad_get_inputs(JOYPAD_PORT_1);
    const joypad_buttons_t now = joypad_get_buttons(JOYPAD_PORT_1);

    /* Do our own rising-edge latch from the raw/current state.  A worked on
       hardware while B did not, despite the portable B->kick path testing
       cleanly; this removes joypad_get_buttons_pressed() from that equation. */
    const bool a_edge = now.a && !g_prev_buttons.a;
    const bool b_edge = now.b && !g_prev_buttons.b;

    out.stick_x = in.stick_x;
    out.stick_y = in.stick_y;
    if (out.stick_x >= -STICK_DEADZONE && out.stick_x <= STICK_DEADZONE &&
        out.stick_y >= -STICK_DEADZONE && out.stick_y <= STICK_DEADZONE) {
        if (now.d_left)  out.stick_x = -90;
        if (now.d_right) out.stick_x =  90;
        if (now.d_down)  out.stick_y = -90;
        if (now.d_up)    out.stick_y =  90;
    }

    out.a = a_edge;
    out.b = b_edge;
    out.z = now.z;
    out.start = now.start && !g_prev_buttons.start;
    out.l = now.l && !g_prev_buttons.l;
    out.r = now.r && !g_prev_buttons.r;
    out.c_up = now.c_up && !g_prev_buttons.c_up;
    out.c_down = now.c_down && !g_prev_buttons.c_down;
    out.c_left = now.c_left && !g_prev_buttons.c_left;
    out.c_right = now.c_right && !g_prev_buttons.c_right;

    if (a_edge) ++g_pad_a_edges;
    if (b_edge) ++g_pad_b_edges;
    g_prev_buttons = now;
    return out;
}

static void fill_rect(int x0, int y0, int x1, int y1, color_t color) {
    rdpq_set_mode_fill(color);
    rdpq_fill_rectangle(x0, y0, x1, y1);
}

static void draw_ring_back(void) {
    fill_rect(0, 0, 320, 240, RGBA32(8, 10, 18, 255));
    fill_rect(0, 82, 320, 240, RGBA32(20, 22, 28, 255));

    fill_rect(26, 96, 294, 214, RGBA32(92, 92, 102, 255));
    fill_rect(33, 102, 287, 205, RGBA32(194, 194, 198, 255));
    fill_rect(39, 108, 281, 199, RGBA32(216, 216, 216, 255));

    fill_rect(25, 83, 31, 205, RGBA32(38, 38, 44, 255));
    fill_rect(289, 83, 295, 205, RGBA32(38, 38, 44, 255));
    fill_rect(29, 105, 291, 108, RGBA32(188, 26, 40, 255));
    fill_rect(29, 114, 291, 117, RGBA32(188, 26, 40, 255));
    fill_rect(29, 123, 291, 126, RGBA32(188, 26, 40, 255));
}

static void draw_ring_front(void) {
    fill_rect(29, 183, 291, 186, RGBA32(202, 28, 44, 255));
    fill_rect(29, 192, 291, 195, RGBA32(202, 28, 44, 255));
    fill_rect(29, 201, 291, 204, RGBA32(202, 28, 44, 255));
    fill_rect(25, 180, 31, 209, RGBA32(38, 38, 44, 255));
    fill_rect(289, 180, 295, 209, RGBA32(38, 38, 44, 255));
}

static void draw_source_sprite(const wm_source_sprite *spr, int anchor_x, int anchor_y,
                               bool flip_x) {
    if (!spr || !spr->pixels_ci8 || !spr->palette_rgba5551 || !spr->palette_colors)
        return;

    surface_t tex = surface_make_linear((void *)spr->pixels_ci8, FMT_CI8,
                                        spr->width, spr->height);

    rdpq_set_mode_standard();
    rdpq_mode_tlut(TLUT_RGBA16);
    rdpq_mode_filter(FILTER_POINT);
    rdpq_mode_alphacompare(1);
    rdpq_tex_upload_tlut(spr->palette_rgba5551, 0, spr->palette_colors);

    /*
     * r6h1: do the CI8 TMEM split explicitly. CI8 + TLUT leaves 2048 bytes
     * of texture TMEM. The generic large-texture blitter is supposed to split
     * for us, but the two-wrestler hardware test showed missing upper strips
     * followed by an RSP/display_get timeout. Keep each submitted blit inside
     * one TMEM-sized horizontal strip so there is no long nested large-blit
     * sequence for the RSP to drain.
     */
    int pitch = (spr->width + 7) & ~7;
    int strip_h = pitch > 0 ? 2048 / pitch : 0;
    if (strip_h < 1)
        strip_h = 1;
    /* Even-height strips keep all non-final T coordinates even, which also
       gives the texture loader its cheapest path where possible. */
    if (strip_h > 2)
        strip_h &= ~1;

    for (int t = 0; t < spr->height; t += strip_h) {
        int h = spr->height - t;
        if (h > strip_h)
            h = strip_h;

        rdpq_tex_blit(&tex, (float)anchor_x, (float)anchor_y,
                      &(rdpq_blitparms_t){
                          .t0 = t,
                          .height = h,
                          .cx = spr->xani,
                          /* cx/cy are relative to the selected sub-rectangle. */
                          .cy = spr->yani - t,
                          .flip_x = flip_x,
                          .filtering = false,
                      });
    }
}

static void draw_shadow(int x, int y) {
    fill_rect(x - 13, y - 3, x + 14, y + 2, RGBA32(86, 86, 90, 255));
}

static const wm_source_sprite *fighter_sprite(const wm_demo_fighter *f,
                                               const wm_visual_frame **frame_out) {
    const wm_visual_frame *frame = wm_visual_current(&f->visual);
    if (frame_out) *frame_out = frame;
    return frame ? wm_bret_sprite_find(frame->source_frame) : NULL;
}

static bool fighter_uses_torso_layer(const wm_demo_fighter *f) {
    /* HRTSEQ1 calls these the WALKING TORSOS. Bret's init also runs one while
       standing. Attack/run composites are left alone until their secondary
       animation semantics are translated. */
    return f->action == WM_DEMO_IDLE || f->action == WM_DEMO_WALK;
}

static void draw_fighter(const wm_demo_fighter *f) {
    const wm_visual_frame *frame = NULL;
    const wm_source_sprite *spr = fighter_sprite(f, &frame);
    draw_shadow(f->screen_x, f->screen_y);
    if (spr) {
        draw_source_sprite(spr, f->screen_x, f->screen_y, f->flip_x);

        if (fighter_uses_torso_layer(f)) {
            const wm_visual_frame *torso_frame = wm_visual_current(&f->torso_visual);
            const wm_source_sprite *torso = torso_frame
                ? wm_bret_sprite_find(torso_frame->source_frame) : NULL;
            if (torso) {
                int a2x = spr->wimp_tail[WM_ATTACH_X_SLOT];
                int a2y = spr->wimp_tail[WM_ATTACH_Y_SLOT];
                if (!(a2x == -1 && a2y == -1) &&
                    a2x >= -512 && a2x <= 512 && a2y >= -512 && a2y <= 512) {
                    int dx = a2x - spr->xani;
                    int dy = a2y - spr->yani;
                    if (f->flip_x)
                        dx = -dx;
                    draw_source_sprite(torso, f->screen_x + dx, f->screen_y + dy, f->flip_x);
                }
            }
        }
    } else {
        fill_rect(f->screen_x - 10, f->screen_y - 48,
                  f->screen_x + 10, f->screen_y, RGBA32(230, 30, 150, 255));
    }
}

static void draw_health_bar(int x, int y, int width, int health, bool right_align) {
    int inner = (width - 4) * health / 100;
    fill_rect(x, y, x + width, y + 8, RGBA32(24, 24, 30, 255));
    fill_rect(x + 2, y + 2, x + width - 2, y + 6, RGBA32(70, 18, 22, 255));
    if (inner <= 0) return;
    if (right_align)
        fill_rect(x + width - 2 - inner, y + 2, x + width - 2, y + 6,
                  RGBA32(228, 210, 72, 255));
    else
        fill_rect(x + 2, y + 2, x + 2 + inner, y + 6,
                  RGBA32(228, 210, 72, 255));
}

static void draw_match_hud(const wm_demo *demo) {
    char line[128];
    draw_health_bar(8, 68, 132, demo->p1.health, false);
    draw_health_bar(180, 68, 132, demo->p2.health, true);
    rdpq_set_mode_standard();
    rdpq_text_print(NULL, 1, 8, 66, "P1 BRET");
    rdpq_text_print(NULL, 1, 248, 66, demo->ai_enabled ? "CPU BRET" : "P2 DUMMY");

    snprintf(line, sizeof(line), "P1 %s %s hp:%d  P2 %s %s hp:%d",
             wm_demo_action_name(demo->p1.action), wm_demo_facing_name(demo->p1.facing),
             demo->p1.health,
             wm_demo_action_name(demo->p2.action), wm_demo_facing_name(demo->p2.facing),
             demo->p2.health);
    rdpq_text_print(NULL, 1, 8, 14, line);

    snprintf(line, sizeof(line), "hits:%u  p1:%u/%u  cpu:%u/%u  AI:%s",
             demo->total_hits,
             demo->p1.hit_count, demo->p1.action_count,
             demo->p2.hit_count, demo->p2.action_count,
             demo->ai_enabled ? "ON" : "OFF");
    rdpq_text_print(NULL, 1, 8, 29, line);

    snprintf(line, sizeof(line), "p1:%d,%d stun:%u  p2:%d,%d stun:%u  VM:%s/%u",
             demo->p1.screen_x, demo->p1.screen_y, demo->p1.stun_ticks,
             demo->p2.screen_x, demo->p2.screen_y, demo->p2.stun_ticks,
             demo->anim.ended ? "END" : "RUN", demo->restarts);
    rdpq_text_print(NULL, 1, 8, 44, line);

    const wm_source_sprite *p1spr = fighter_sprite(&demo->p1, NULL);
    if (p1spr) {
        int a2x = p1spr->wimp_tail[WM_ATTACH_X_SLOT];
        int a2y = p1spr->wimp_tail[WM_ATTACH_Y_SLOT];
        snprintf(line, sizeof(line), "PAD A:%u B:%u  A2:+38/+40 v:%d,%d",
                 g_pad_a_edges, g_pad_b_edges, a2x, a2y);
        rdpq_text_print(NULL, 1, 8, 57, line);
    }
}

static void render(const wm_demo *demo, bool connected, bool show_debug) {
    surface_t *disp = display_get();
    rdpq_attach(disp, NULL);
    draw_ring_back();

    /* Painter-style depth ordering: farther fighter first, nearer fighter last. */
    if (demo->p1.screen_y <= demo->p2.screen_y) {
        draw_fighter(&demo->p1);
        draw_fighter(&demo->p2);
    } else {
        draw_fighter(&demo->p2);
        draw_fighter(&demo->p1);
    }

    draw_ring_front();
    if (show_debug)
        draw_match_hud(demo);
    else {
        rdpq_set_mode_standard();
        rdpq_text_print(NULL, 1, 8, 14, "WM ARCADE -> N64 r7   START: debug HUD");
    }

    rdpq_set_mode_standard();
    rdpq_text_print(NULL, 1, 8, 226,
                    connected ? "Move Z+run  A punch  B kick  L AI  R reset  Start HUD"
                              : "NO P1 PAD   CPU sandbox running");

    /*
     * Keep this hardware-debug build synchronous. r6 used detach_show(), which
     * lets the CPU queue another frame while RSP/RDP are still chewing on the
     * previous one. With two large CI8 wrestlers the real N64 eventually hit
     * display_get's RSP wait timeout. Finish the frame before returning the
     * framebuffer so queue backlog cannot accumulate.
     */
    rdpq_detach_wait();
    display_show(disp);
}

int main(void) {
    wm_demo demo;
    bool show_debug = true;

    debug_init_emulog();
    debug_init_usblog();
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2,
                 GAMMA_NONE, FILTERS_RESAMPLE);
    rdpq_init();
    /* Enable the RDP command validator for this hardware-debug revision. */
    rdpq_debug_start();
    joypad_init();
    rdpq_text_register_font(1, rdpq_font_load_builtin(FONT_BUILTIN_DEBUG_VAR));

    wm_demo_init(&demo);
    debugf("wm_arcade_port r7: locked attachment + hardware B-edge input\n");
    debugf("embedded source sprites: %u\n", (unsigned)wm_bret_sprite_count());

    while (1) {
        bool connected = false;
        wm_input_state input = read_input(&connected);
        if (input.start)
            show_debug = !show_debug;
        wm_demo_tick(&demo, &input);
        render(&demo, connected, show_debug);
    }
}
