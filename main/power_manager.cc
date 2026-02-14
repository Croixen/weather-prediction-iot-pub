#include "power_manager.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h" 
#include "button_handler.h"
#include <sys/time.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h> 

#define TAG "[POWER]"

static RTC_DATA_ATTR power_manager_state_t pm_state = {
    .last_inference_time_ms = 0,
    .next_inference_time_ms = 0,
    .current_display_page = 0,
    .inference_completed = false,
    .wake_count = 0
};

static uint64_t boot_time_ms = 0;

esp_err_t power_manager_init(void) {
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    
    boot_time_ms = esp_timer_get_time() / 1000;  // Convert to ms
    pm_state.wake_count++;

    ESP_LOGI(TAG, "Power manager initialized (wake #%lu)", pm_state.wake_count);
    ESP_LOGI(TAG, "Wake cause: %d", cause);
    
    esp_sleep_enable_ext0_wakeup(BTN_NEXT_GPIO, 0);  // Wake on LOW (button pressed)
    
    return ESP_OK;
}

wake_cause_t power_manager_get_wake_cause(void) {
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    
    switch (cause) {
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "Woke from timer");
            return WAKE_CAUSE_TIMER;
            
        case ESP_SLEEP_WAKEUP_EXT0:
        case ESP_SLEEP_WAKEUP_EXT1:
            ESP_LOGI(TAG, "Woke from button");
            return WAKE_CAUSE_BUTTON;
            
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            ESP_LOGI(TAG, "Woke from reset/power-on");
            return WAKE_CAUSE_RESET;
            
        default:
            return WAKE_CAUSE_UNKNOWN;
    }
}

uint64_t power_manager_get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

static uint64_t get_system_time_seconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec;
}

uint32_t power_manager_get_minutes_into_hour(void) {
    uint64_t seconds = get_system_time_seconds();
    uint32_t seconds_into_hour = seconds % 3600;
    return seconds_into_hour / 60;
}

uint64_t power_manager_calculate_sleep_duration(void) {
    uint64_t current_time_ms = power_manager_get_time_ms();
    
    if (pm_state.next_inference_time_ms == 0) {
        pm_state.next_inference_time_ms = current_time_ms + (ONE_HOUR_US * 1000);
    }
    
    int64_t remaining_ms = pm_state.next_inference_time_ms - current_time_ms;
    
    if (remaining_ms <= 0) {
        ESP_LOGI(TAG, "Inference overdue by %lld ms, sleeping briefly to reset", -remaining_ms);
        return 1000000ULL; 
    }
    
    ESP_LOGI(TAG, "Smart Sleep: Current %llu, Next %llu", current_time_ms, pm_state.next_inference_time_ms);
    ESP_LOGI(TAG, "Sleeping for remaining %lld seconds", remaining_ms / 1000);
    
    return (uint64_t)remaining_ms * 1000ULL; 
}

void power_manager_update_inference_time(void) {
    uint64_t current_time_ms = power_manager_get_time_ms();
    
    pm_state.last_inference_time_ms = current_time_ms;
    pm_state.next_inference_time_ms = current_time_ms + (INFERENCE_INTERVAL_SECONDS * 1000);
    pm_state.inference_completed = true;
    
    ESP_LOGI(TAG, "Inference completed. Next run in %d seconds (at %llu)", 
             INFERENCE_INTERVAL_SECONDS, pm_state.next_inference_time_ms);
}

void power_manager_save_data(const t_infered* prediction, const t_bme280_s_val* sensor_data, bool wifi_connected, const char* ip_address) {
    if (prediction != NULL) {
        pm_state.saved_prediction = *prediction;
    }
    if (sensor_data != NULL) {
        pm_state.saved_sensor_data = *sensor_data;
    }
    
    pm_state.saved_wifi_connected = wifi_connected;
    if (ip_address != NULL) {
        strncpy(pm_state.saved_ip_address, ip_address, sizeof(pm_state.saved_ip_address) - 1);
        pm_state.saved_ip_address[sizeof(pm_state.saved_ip_address) - 1] = '\0';
    } else {
        pm_state.saved_ip_address[0] = '\0';
    }
    
    ESP_LOGI(TAG, "Data saved to RTC memory");
}

void power_manager_get_data(t_infered* prediction, t_bme280_s_val* sensor_data, bool* wifi_connected, char* ip_address) {
    if (prediction != NULL) {
        *prediction = pm_state.saved_prediction;
    }
    if (sensor_data != NULL) {
        *sensor_data = pm_state.saved_sensor_data;
    }
    if (wifi_connected != NULL) {
        *wifi_connected = pm_state.saved_wifi_connected;
    }
    if (ip_address != NULL) {
        strncpy(ip_address, pm_state.saved_ip_address, 16);
    }
}

bool power_manager_should_run_inference(void) {
    uint64_t current_time_ms = power_manager_get_time_ms();
    
    if (pm_state.next_inference_time_ms == 0) {
        return true;
    }
    
    wake_cause_t cause = power_manager_get_wake_cause();
    if (cause == WAKE_CAUSE_TIMER) {
        ESP_LOGI(TAG, "Inference due (Wake Cause: TIMER)");
        return true;
    }
    if (cause == WAKE_CAUSE_RESET) {
        ESP_LOGI(TAG, "Inference due (Wake Cause: RESET)");
        return true;
    }
    
    bool is_due = (current_time_ms >= pm_state.next_inference_time_ms);
    
    ESP_LOGI(TAG, "Inference check: %s (Current: %llu, Target: %llu)", 
             is_due ? "DUE" : "WAITING", current_time_ms, pm_state.next_inference_time_ms);
             
    return is_due;
}

uint8_t power_manager_get_display_page(void) {
    if (pm_state.current_display_page > 5) {
        pm_state.current_display_page = 0;
    }
    return pm_state.current_display_page;
}

void power_manager_set_display_page(uint8_t page) {
    if (page > 5) {
        page = 5;
    }
    pm_state.current_display_page = page;
    ESP_LOGI(TAG, "Display page set to: %d", page);
}

void power_manager_enter_deep_sleep(void) {
    uint64_t sleep_duration_us = power_manager_calculate_sleep_duration();
    
    ESP_LOGI(TAG, "Entering deep sleep for %llu seconds", sleep_duration_us / 1000000);
    
    esp_sleep_enable_timer_wakeup(sleep_duration_us);
    
    if (rtc_gpio_is_valid_gpio(BTN_NEXT_GPIO)) {
        rtc_gpio_pullup_en(BTN_NEXT_GPIO);
        rtc_gpio_pulldown_dis(BTN_NEXT_GPIO);
        esp_sleep_enable_ext0_wakeup(BTN_NEXT_GPIO, 0); // Wake on LOW
    }
    
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    esp_deep_sleep_start();
}

void power_manager_enter_deep_sleep_now(void) {
    ESP_LOGI(TAG, "Entering immediate deep sleep (user triggered)");
    power_manager_enter_deep_sleep();
}
