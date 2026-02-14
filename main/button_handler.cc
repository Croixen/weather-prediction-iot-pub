#include "button_handler.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "display_driver.h"

#define TAG "[BUTTON]"

static button_handler_t btn_handler = {0};

esp_err_t button_init(void) {
    gpio_config_t io_conf = {};
    
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BTN_NEXT_GPIO) | (1ULL << BTN_PREV_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure button GPIOs");
        return ret;
    }

    btn_handler.next_pressed = false;
    btn_handler.prev_pressed = false;
    btn_handler.both_pressed = false;
    btn_handler.next_press_time = 0;
    btn_handler.prev_press_time = 0;
    btn_handler.both_press_time = 0;
    btn_handler.last_activity_time = esp_timer_get_time() / 1000;  // Convert to ms
    btn_handler.long_press_warning_shown = false;

    ESP_LOGI(TAG, "Button handler initialized");
    return ESP_OK;
}

button_event_t button_poll(void) {
    uint64_t current_time = esp_timer_get_time() / 1000;  // Convert to ms
    
    bool next_is_pressed = (gpio_get_level(BTN_NEXT_GPIO) == 0);
    bool prev_is_pressed = (gpio_get_level(BTN_PREV_GPIO) == 0);
    
    button_event_t event = BTN_EVENT_NONE;

    if (next_is_pressed && prev_is_pressed) {
        if (!btn_handler.both_pressed) {
            btn_handler.both_pressed = true;
            btn_handler.both_press_time = current_time;
            btn_handler.last_activity_time = current_time;
            btn_handler.long_press_warning_shown = false;
            
            ESP_LOGI(TAG, "Both buttons pressed");
            event = BTN_EVENT_BOTH_PRESSED;
        } else {
            uint64_t press_duration = current_time - btn_handler.both_press_time;
            
            if (press_duration >= BUTTON_LONG_PRESS_MS && !btn_handler.long_press_warning_shown) {
                ESP_LOGI(TAG, "Long press detected (>5s)");
                btn_handler.long_press_warning_shown = true;
                event = BTN_EVENT_BOTH_LONG_PRESS;
            }
        }
        
        btn_handler.next_pressed = true;
        btn_handler.prev_pressed = true;
        
    } 

    else if (!next_is_pressed && !prev_is_pressed) {
        if (btn_handler.both_pressed && btn_handler.long_press_warning_shown) {
            ESP_LOGI(TAG, "Buttons released after long press warning");
            event = BTN_EVENT_BOTH_RELEASED_AFTER_LONG;
        }
        
        btn_handler.next_pressed = false;
        btn_handler.prev_pressed = false;
        btn_handler.both_pressed = false;
        btn_handler.long_press_warning_shown = false;
    }

    else if (next_is_pressed && !prev_is_pressed) {
        if (!btn_handler.next_pressed && !btn_handler.both_pressed) {
            if (current_time - btn_handler.next_press_time >= BUTTON_DEBOUNCE_MS) {
                btn_handler.next_pressed = true;
                btn_handler.next_press_time = current_time;
                btn_handler.last_activity_time = current_time;
                
                ESP_LOGI(TAG, "NEXT button pressed");
                event = BTN_EVENT_NEXT_PRESSED;
            }
        }
        
        btn_handler.prev_pressed = false;
        btn_handler.both_pressed = false;
    }

    else if (!next_is_pressed && prev_is_pressed) {
        if (!btn_handler.prev_pressed && !btn_handler.both_pressed) {
            if (current_time - btn_handler.prev_press_time >= BUTTON_DEBOUNCE_MS) {
                btn_handler.prev_pressed = true;
                btn_handler.prev_press_time = current_time;
                btn_handler.last_activity_time = current_time;
                
                ESP_LOGI(TAG, "PREV button pressed");
                event = BTN_EVENT_PREV_PRESSED;
            }
        }
        
        btn_handler.next_pressed = false;
        btn_handler.both_pressed = false;
    }

    return event;
}

uint64_t button_get_time_since_last_activity(void) {
    uint64_t current_time = esp_timer_get_time() / 1000;  // Convert to ms
    return current_time - btn_handler.last_activity_time;
}

void button_reset_inactivity_timer(void) {
    btn_handler.last_activity_time = esp_timer_get_time() / 1000;
}

bool button_is_inactive_for(uint64_t duration_ms) {
    return button_get_time_since_last_activity() >= duration_ms;
}
