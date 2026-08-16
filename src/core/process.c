#include "wm/process.h"
#include <string.h>

void wm_scheduler_init(wm_scheduler *s) {
    memset(s, 0, sizeof(*s));
}

wm_process *wm_process_create(wm_scheduler *s, uint16_t id, wm_process_fn fn, void *user) {
    for (size_t i = 0; i < WM_MAX_PROCESSES; ++i) {
        wm_process *p = &s->slots[i];
        if (!p->active) {
            *p = (wm_process){0};
            p->id = id;
            p->active = true;
            p->wake_tick = s->tick;
            p->fn = fn;
            p->user = user;
            return p;
        }
    }
    return NULL;
}

void wm_process_kill(wm_process *p) {
    if (p) p->active = false;
}

void wm_process_sleep(wm_scheduler *s, wm_process *p, uint32_t ticks) {
    if (p) p->wake_tick = s->tick + ticks;
}

void wm_scheduler_step(wm_scheduler *s) {
    for (size_t i = 0; i < WM_MAX_PROCESSES; ++i) {
        wm_process *p = &s->slots[i];
        if (p->active && p->fn && p->wake_tick <= s->tick) {
            p->fn(p, p->user);
        }
    }
    ++s->tick;
}
