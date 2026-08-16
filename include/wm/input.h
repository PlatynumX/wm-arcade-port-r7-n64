#ifndef WM_INPUT_H
#define WM_INPUT_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int8_t stick_x;
    int8_t stick_y;
    /* Edge-triggered action/menu buttons unless noted. */
    bool a;
    bool b;
    bool z;      /* held: run modifier */
    bool start;
    bool l;
    bool r;
    bool c_up;
    bool c_down;
    bool c_left;
    bool c_right;
} wm_input_state;

#endif
