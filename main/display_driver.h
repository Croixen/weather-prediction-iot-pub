#pragma once

#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include "data_structure.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

// Display page indices
typedef enum {
    DISPLAY_PAGE_0 = 0,  // Current hour
    DISPLAY_PAGE_1 = 1,  // +1 hour
    DISPLAY_PAGE_2 = 2,  // +2 hours
    DISPLAY_PAGE_3 = 3,  // +3 hours
    DISPLAY_PAGE_4 = 4,  // +4 hours
    DISPLAY_PAGE_5 = 5,  // +5 hours
    DISPLAY_PAGE_6 = 6,  // +6 hours
    DISPLAY_PAGE_MAX = 6
} display_page_t;

// Initialize the display driver
esp_err_t display_init(void);

// Display a page (0 = Current, 1-5 = Prediction)
void display_show_page(uint8_t page_index, const t_infered* prediction_data, const t_bme280_s_val* sensor_data);

// Display network status (connected/disconnected with IP and buffer size)
void display_show_network_status(bool connected, const char* ip_address, int buffer_size);

// Display sleep warning screen
void display_show_sleep_warning(void);

// Clear display
void display_clear(void);

// Turn display on (wake from sleep)
void display_on(void);

// Turn display off (sleep mode)
void display_off(void);

// Check if display is currently on
bool display_is_on(void);

#endif // DISPLAY_DRIVER_H
