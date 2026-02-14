#pragma once

#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include "driver/gpio.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#define BTN_NEXT_GPIO    GPIO_NUM_8  // D1
#define BTN_PREV_GPIO    GPIO_NUM_7  // D0

#define BUTTON_DEBOUNCE_MS      50

#define BUTTON_LONG_PRESS_MS    5000

typedef enum {
    BTN_STATE_RELEASED = 0,
    BTN_STATE_PRESSED = 1,
    BTN_STATE_LONG_PRESS = 2
} button_state_t;

typedef enum {
    BTN_EVENT_NONE = 0,
    BTN_EVENT_NEXT_PRESSED,
    BTN_EVENT_PREV_PRESSED,
    BTN_EVENT_BOTH_PRESSED,
    BTN_EVENT_BOTH_LONG_PRESS,
    BTN_EVENT_BOTH_RELEASED_AFTER_LONG
} button_event_t;

typedef struct {
    bool next_pressed;
    bool prev_pressed;
    bool both_pressed;
    uint64_t next_press_time;
    uint64_t prev_press_time;
    uint64_t both_press_time;
    uint64_t last_activity_time;
    bool long_press_warning_shown;
} button_handler_t;

esp_err_t button_init(void);

button_event_t button_poll(void);

uint64_t button_get_time_since_last_activity(void);

void button_reset_inactivity_timer(void);

bool button_is_inactive_for(uint64_t duration_ms);

#endif // BUTTON_HANDLER_H
