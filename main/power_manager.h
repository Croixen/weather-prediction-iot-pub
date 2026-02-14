#pragma once

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include "esp_err.h"
#include "esp_sleep.h"
#include <stdint.h>
#include <stdbool.h>
#include "data_structure.h" // Added for t_infered

// Sleep configuration
#define INACTIVITY_TIMEOUT_MS       7000    // 7 seconds
#define INFERENCE_INTERVAL_SECONDS  60       // Debug
#define ONE_HOUR_US                 (60ULL * 1000000ULL) // Productions

// Wake-up sources
typedef enum {
    WAKE_CAUSE_TIMER = 0,
    WAKE_CAUSE_BUTTON,
    WAKE_CAUSE_RESET,
    WAKE_CAUSE_UNKNOWN
} wake_cause_t;

// Power manager state (persisted in RTC memory)
typedef struct {
    uint64_t last_inference_time_ms;     // Time of last inference (in milliseconds since epoch)
    uint64_t next_inference_time_ms;     // Time of next scheduled inference
    uint8_t  current_display_page;       // Current display page (0-5)
    bool     inference_completed;        // Flag indicating if inference was completed
    uint32_t wake_count;                 // Number of wake-ups (for debugging)
    t_infered saved_prediction;          // Saved prediction results
    t_bme280_s_val saved_sensor_data;    // Saved current sensor reading
    bool saved_wifi_connected;           // Saved WiFi status
    char saved_ip_address[16];           // Saved IP address string
} power_manager_state_t;

// Initialize power manager
esp_err_t power_manager_init(void);

// Get wake-up cause
wake_cause_t power_manager_get_wake_cause(void);

// Calculate sleep duration until next inference
uint64_t power_manager_calculate_sleep_duration(void);

// Update inference timing after running inference
void power_manager_update_inference_time(void);

// Save the latest prediction, sensor data, and network status to RTC memory
void power_manager_save_data(const t_infered* prediction, const t_bme280_s_val* sensor_data, bool wifi_connected, const char* ip_address);

// Load the saved data from RTC memory
void power_manager_get_data(t_infered* prediction, t_bme280_s_val* sensor_data, bool* wifi_connected, char* ip_address);

// Check if inference should be run now
bool power_manager_should_run_inference(void);

// Get current display page (restored from RTC memory)
uint8_t power_manager_get_display_page(void);

// Set current display page (saved to RTC memory)
void power_manager_set_display_page(uint8_t page);

// Enter deep sleep with calculated duration
void power_manager_enter_deep_sleep(void);

// Enter deep sleep immediately (for button-triggered sleep)
void power_manager_enter_deep_sleep_now(void);

// Get time in milliseconds since boot
uint64_t power_manager_get_time_ms(void);

// Get minutes into current hour
uint32_t power_manager_get_minutes_into_hour(void);

#endif // POWER_MANAGER_H
