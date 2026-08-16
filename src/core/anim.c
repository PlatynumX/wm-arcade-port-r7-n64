#include "wm/anim.h"
#include <string.h>

static bool take_word(wm_anim_state *a, uint16_t *out) {
    if (a->pc >= a->word_count) return false;
    *out = a->words[a->pc++];
    return true;
}

void wm_anim_start(wm_anim_state *a, const uint16_t *words, size_t word_count) {
    memset(a, 0, sizeof(*a));
    a->words = words;
    a->word_count = word_count;
    a->speed = 0x100;
}

bool wm_anim_step(wm_anim_state *a, wm_object *obj) {
    if (a->ended) return false;
    if (a->pause_ticks) {
        --a->pause_ticks;
        return true;
    }

    /* Run commands until one yields (PAUSE) or terminates. */
    while (a->pc < a->word_count) {
        uint16_t op;
        if (!take_word(a, &op)) break;
        switch (op) {
            case WM_ANI_SETMODE: {
                uint16_t v;
                if (!take_word(a, &v)) goto malformed;
                a->mode = v;
                obj->visible = (v & WM_MODE_INVISIBLE) == 0;
                break;
            }
            case WM_ANI_ZEROVELS:
                obj->vx = obj->vy = obj->vz = 0;
                break;
            case WM_ANI_ZERO_XZVELS:
                obj->vx = obj->vz = 0;
                break;
            case WM_ANI_SET_YVEL: {
                uint16_t raw;
                if (!take_word(a, &raw)) goto malformed;
                obj->vy = (int16_t)raw;
                break;
            }
            case WM_ANI_SET_XVEL: {
                uint16_t raw;
                if (!take_word(a, &raw)) goto malformed;
                obj->vx = (int16_t)raw;
                break;
            }
            case WM_ANI_SET_ZVEL: {
                uint16_t raw;
                if (!take_word(a, &raw)) goto malformed;
                obj->vz = (int16_t)raw;
                break;
            }
            case WM_ANI_SETSPEED: {
                uint16_t v;
                if (!take_word(a, &v)) goto malformed;
                a->speed = v;
                break;
            }
            case WM_ANI_SETFACING:
                /* Exact facing semantics depend on player/opponent state. */
                break;
            case WM_ANI_SET_WRESTLER_XFLIP:
                /* Original command derives image flip from wrestler/facing state.
                   Keep it explicit but inert until player-facing state is ported. */
                break;
            case WM_ANI_PAUSE: {
                uint16_t ticks;
                if (!take_word(a, &ticks)) goto malformed;
                a->pause_ticks = ticks;
                return true;
            }
            case WM_ANI_END:
                a->mode |= WM_MODE_END;
                a->ended = true;
                return false;
            default:
                /* Deliberately stop on untranslated commands instead of silently
                   interpreting source data incorrectly. */
                a->ended = true;
                return false;
        }
    }

malformed:
    a->ended = true;
    return false;
}
