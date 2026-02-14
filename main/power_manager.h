#pragma once

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include "esp_err.h"
#include "esp_sleep.h"
#include <stdint.h>
#include <stdbool.h>
#include "data_structure.h" 

// Sleep configuration
#define INACTIVITY_TIMEOUT_MS       7000    
#define INFERENCE_INTERVAL_SECONDS  60 * 60       
#define ONE_HOUR_US                 (3600ULL * 1000000ULL) 

typedef enum {
    WAKE_CAUSE_TIMER = 0,
    WAKE_CAUSE_BUTTON,
    WAKE_CAUSE_RESET,
    WAKE_CAUSE_UNKNOWN
} wake_cause_t;

typedef struct {
    uint64_t last_inference_time_ms;     
    uint64_t next_inference_time_ms;     
    uint8_t  current_display_page;       
    bool     inference_completed;        
    uint32_t wake_count;                 
    t_infered saved_prediction;          
    t_bme280_s_val saved_sensor_data;    
    bool saved_wifi_connected;           
    char saved_ip_address[16];           
} power_manager_state_t;

esp_err_t power_manager_init(void);

wake_cause_t power_manager_get_wake_cause(void);

uint64_t power_manager_calculate_sleep_duration(void);

void power_manager_update_inference_time(void);

void power_manager_save_data(const t_infered* prediction, const t_bme280_s_val* sensor_data, bool wifi_connected, const char* ip_address);

void power_manager_get_data(t_infered* prediction, t_bme280_s_val* sensor_data, bool* wifi_connected, char* ip_address);

bool power_manager_should_run_inference(void);

uint8_t power_manager_get_display_page(void);

void power_manager_set_display_page(uint8_t page);

void power_manager_enter_deep_sleep(void);

void power_manager_enter_deep_sleep_now(void);

uint64_t power_manager_get_time_ms(void);

uint32_t power_manager_get_minutes_into_hour(void);

#endif 
