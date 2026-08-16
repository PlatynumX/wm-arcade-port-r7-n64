#ifndef WM_BRET_VISUALS_H
#define WM_BRET_VISUALS_H

#include "wm/visual.h"

/* Movement / stance streams from HRTSEQ1.ASM. */
extern const wm_visual_sequence wm_bret_stand2_anim;
extern const wm_visual_sequence wm_bret_stand4_anim;
/* The arcade keeps a second, independently animated torso layer. */
extern const wm_visual_sequence wm_bret_torso2_anim;
extern const wm_visual_sequence wm_bret_torso4_anim;
extern const wm_visual_sequence wm_bret_walk2_f2_anim;
extern const wm_visual_sequence wm_bret_walk8_f2_anim;
extern const wm_visual_sequence wm_bret_walk4_f4_anim;
extern const wm_visual_sequence wm_bret_walk6_f4_anim;
extern const wm_visual_sequence wm_bret_run_anim;

/* Linear visual slices of the normal attack paths from HRTSEQ2.ASM.
   Gameplay hit/branch semantics are intentionally not claimed yet. */
extern const wm_visual_sequence wm_bret_punch2_anim;
extern const wm_visual_sequence wm_bret_punch4_anim;
extern const wm_visual_sequence wm_bret_kick2_anim;
extern const wm_visual_sequence wm_bret_kick4_anim;

#endif
