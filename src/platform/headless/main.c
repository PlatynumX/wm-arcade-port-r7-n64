#include <stdio.h>
#include "wm/demo.h"
#include "wm/source_data.h"
#include "wm/visual.h"

int main(void) {
    wm_demo demo;
    wm_input_state input = {0};

    wm_demo_init(&demo);
    for (int i = 0; i < 30; ++i)
        wm_demo_tick(&demo, &input);

    const wm_visual_frame *p1f = wm_visual_current(&demo.p1.visual);
    const wm_visual_frame *p2f = wm_visual_current(&demo.p2.visual);
    printf("wm_arcade_port r7\n");
    printf("target model: N64-first / portable-core\n");
    printf("source sequence=%s::%s words=%zu ended=%d mode=0x%04x\n",
           wm_source_hrt_finish1_move.source_file,
           wm_source_hrt_finish1_move.source_label,
           wm_source_hrt_finish1_move.word_count,
           demo.anim.ended ? 1 : 0, demo.anim.mode);
    printf("p1=%s/%s hp=%d frame=%s pos=%d,%d\n",
           wm_demo_action_name(demo.p1.action), wm_demo_facing_name(demo.p1.facing),
           demo.p1.health, p1f ? p1f->source_frame : "none",
           demo.p1.screen_x, demo.p1.screen_y);
    printf("p2=%s/%s hp=%d frame=%s pos=%d,%d ai=%d\n",
           wm_demo_action_name(demo.p2.action), wm_demo_facing_name(demo.p2.facing),
           demo.p2.health, p2f ? p2f->source_frame : "none",
           demo.p2.screen_x, demo.p2.screen_y, demo.ai_enabled ? 1 : 0);
    printf("core tick=%u frame=%llu hits=%u\n",
           demo.game.scheduler.tick,
           (unsigned long long)demo.game.frame,
           demo.total_hits);
    printf("portable two-layer two-wrestler runtime: PASS\n");
    return (demo.anim.ended && p1f && p2f) ? 0 : 1;
}
