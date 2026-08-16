#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ORIG="$ROOT/original/wwf-wrestlemania"
if [ ! -f "$ORIG/ANIM.EQU" ] || [ ! -f "$ORIG/FINISEQ.ASM" ] || \
   [ ! -f "$ORIG/HRTSEQ1.ASM" ] || [ ! -f "$ORIG/HRTSEQ2.ASM" ]; then
    sh "$ROOT/scripts/fetch_original.sh"
fi
python3 "$ROOT/tools/asmseq.py" \
    --equ "$ORIG/ANIM.EQU" \
    --source "$ORIG/FINISEQ.ASM" \
    --label hrt_finish1_move \
    --symbol wm_seq_hrt_finish1_move \
    --out "$ROOT/src/generated/finish_sequences.c"
python3 "$ROOT/tools/wlanim.py" \
    --source "$ORIG/HRTSEQ1.ASM" \
    --sequence hrt_stand2_anim wm_bret_stand2_anim bret_stand2_frames \
    --sequence hrt_stand4_anim wm_bret_stand4_anim bret_stand4_frames \
    --sequence hrt_torso2_anim wm_bret_torso2_anim bret_torso2_frames \
    --sequence hrt_torso4_anim wm_bret_torso4_anim bret_torso4_frames \
    --sequence hrt_walk2_f2_anim wm_bret_walk2_f2_anim bret_walk2_f2_frames \
    --sequence hrt_walk8_f2_anim wm_bret_walk8_f2_anim bret_walk8_f2_frames \
    --sequence hrt_walk4_f4_anim wm_bret_walk4_f4_anim bret_walk4_f4_frames \
    --sequence hrt_walk6_f4_anim wm_bret_walk6_f4_anim bret_walk6_f4_frames \
    --slice hrt_run_anim wm_bret_run_anim bret_run_frames true \
    --out "$ROOT/src/generated/bret_visuals.c"
python3 "$ROOT/tools/wlanim.py" \
    --source "$ORIG/HRTSEQ2.ASM" \
    --slice hrt_2_punch_anim wm_bret_punch2_anim bret_punch2_frames false \
    --slice hrt_4_punch_anim wm_bret_punch4_anim bret_punch4_frames false \
    --slice hrt_2_kick_anim wm_bret_kick2_anim bret_kick2_frames false \
    --slice hrt_4_kick_anim wm_bret_kick4_anim bret_kick4_frames false \
    --out "$ROOT/src/generated/bret_attacks.c"
