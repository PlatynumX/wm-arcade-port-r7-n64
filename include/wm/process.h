#ifndef WM_PROCESS_H
#define WM_PROCESS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WM_MAX_PROCESSES 64

typedef struct wm_process wm_process;
typedef void (*wm_process_fn)(wm_process *proc, void *user);

struct wm_process {
    uint16_t id;
    bool active;
    uint32_t wake_tick;
    uint32_t state;
    wm_process_fn fn;
    void *user;
};

typedef struct {
    uint32_t tick;
    wm_process slots[WM_MAX_PROCESSES];
} wm_scheduler;

void wm_scheduler_init(wm_scheduler *s);
wm_process *wm_process_create(wm_scheduler *s, uint16_t id, wm_process_fn fn, void *user);
void wm_process_kill(wm_process *p);
void wm_process_sleep(wm_scheduler *s, wm_process *p, uint32_t ticks);
void wm_scheduler_step(wm_scheduler *s);

#endif
