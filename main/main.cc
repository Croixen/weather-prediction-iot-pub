#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bme280_driver.h"
#include "data_structure.h"
#include "circle_buffer.h"
#include "inference_data.h"
#include "wifi_helper.h"
#include "mqtt_helper.h"
#include "display_driver.h"
#include "button_handler.h"
#include "power_manager.h"

#define TAG "MAIN"

#define PASSWORD "REDACTED"
#define SSID "REDACTED"

static uint8_t current_page = 0;
static bool network_status_displayed = false;
static bool sleep_warning_displayed = false;

static esp_err_t system_init(void) {
    esp_err_t ret;
    
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "Initializing power manager...");
    ret = power_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize power manager");
        return ret;
    }
    
    ESP_LOGI(TAG, "Initializing I2C...");
    initialize_i2c();
    vTaskDelay(pdMS_TO_TICKS(50));
    
    ESP_LOGI(TAG, "Initializing display...");
    ret = display_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return ret;
    }
    
    ESP_LOGI(TAG, "Initializing buttons...");
    ret = button_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize buttons");
        return ret;
    }
    
    init_circular_buffer();
    
    current_page = power_manager_get_display_page();
    ESP_LOGI(TAG, "Restored display page: %d", current_page);
    
    return ESP_OK;
}

static esp_err_t run_inference(t_infered* prediction) {
    ESP_LOGI(TAG, "=== Running Inference ===");
    
    init_interpeter();
    vTaskDelay(pdMS_TO_TICKS(10));
    
    esp_err_t ret = inference_invoke();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Inference failed");
        return ret;
    }
    
    *prediction = prediction_result;
    
    power_manager_update_inference_time();
    
    ESP_LOGI(TAG, "Inference completed successfully");
    ESP_LOGI(TAG, "Tensor usage: %d bytes", prediction->tensor_usage);
    ESP_LOGI(TAG, "Inference time: %lld us", prediction->time);
    
    return ESP_OK;
}

static void handle_network(t_bme280_s_val* sensor_data, t_infered* prediction, bool has_prediction) {
    ESP_LOGI(TAG, "=== Network Communication ===");
    
    wifi_init();
    wifi_connect(SSID, PASSWORD);
    
    init_mqtt();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    send_sensor_value(sensor_data, last_reset_code);
    
    if (has_prediction) {
        expected_message = 2;
        send_inference_value(prediction);
    } else {
        expected_message = 1;
    }
    
    TickType_t wait_time = pdMS_TO_TICKS(10000);
    while (sent_message < expected_message && wait_time > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_time -= pdMS_TO_TICKS(100);
    }
    
    if (sent_message >= expected_message) {
        ESP_LOGI(TAG, "All data sent successfully");
    } else {
        ESP_LOGW(TAG, "MQTT send timeout");
    }
    
    vTaskDelay(pdMS_TO_TICKS(1000));
}

extern "C" void app_main() {
    ESP_LOGI(TAG, "\n\n========================================");
    ESP_LOGI(TAG, "Weather Prediction System Starting");
    ESP_LOGI(TAG, "========================================\n");
    
    ESP_ERROR_CHECK(system_init());
    
    wake_cause_t wake_cause = power_manager_get_wake_cause();
    
    if (last_reset_code == ESP_RST_BROWNOUT) {
        ESP_LOGE(TAG, "Brownout detected! Entering protective sleep");
        power_manager_enter_deep_sleep_now();
        return;
    }
    
    t_bme280_s_val sensor_data = {0};
    t_infered prediction = {0};
    esp_err_t inference_result = ESP_FAIL;
    bool skip_data_collection = false;
    bool wifi_connected = false;
    char ip_address[16] = "0.0.0.0";

    bool is_inference_due = power_manager_should_run_inference();
    
    if (wake_cause == WAKE_CAUSE_BUTTON && !is_inference_due) {
        ESP_LOGI(TAG, "Woken by button (Inference NOT due) - Entering interactive mode");
        skip_data_collection = true;
        
        power_manager_get_data(&prediction, &sensor_data, &wifi_connected, ip_address);
        
        if (size >= 24 && prediction.time == 0) {
            ESP_LOGI(TAG, "Buffer full (24) but no valid prediction. Forcing inference...");
            inference_result = run_inference(&prediction);
            power_manager_save_data(&prediction, &sensor_data, wifi_connected, ip_address);
        }
        
        // Assume inference is OK if we have valid saved data (simple check: time != 0)
        if (prediction.time != 0) {
            inference_result = ESP_OK;
            ESP_LOGI(TAG, "Loaded saved prediction from RTC");
        } else {
            ESP_LOGW(TAG, "No saved prediction found in RTC");
        }
    } else {
        ESP_LOGI(TAG, "Inference Due (or Timer Wake) - Proceeding with Data Collection");
        skip_data_collection = false;
    }
    
    if (!skip_data_collection) {
        read_sensor(&sensor_data);
        vTaskDelay(pdMS_TO_TICKS(10));
        
        if (sensor_data.temperature < 0) {
            ESP_LOGE(TAG, "Sensor read failed");
            vTaskDelay(pdMS_TO_TICKS(3000));
            power_manager_enter_deep_sleep();
            return;
        }
        
        ESP_LOGI(TAG, "Sensor: T=%.1f°C, H=%.1f%%, P=%.0fhPa", 
                 sensor_data.temperature, sensor_data.humidity, sensor_data.pressure);
        
        push_data_into_stack(sensor_data);
        vTaskDelay(pdMS_TO_TICKS(10));
        
        bool should_infer = power_manager_should_run_inference();
        bool inference_ready = (size >= 24);  // Need 24 data points
        bool run_inference_now = should_infer && inference_ready;
        
        if (run_inference_now) {
            inference_result = run_inference(&prediction);
        } else {
            if (!inference_ready) {
                ESP_LOGI(TAG, "Not enough data for inference (%d/24)", size);
            } else {
                ESP_LOGI(TAG, "Inference not scheduled at this time");
            }
        }
        
        ESP_LOGI(TAG, "=== Network Communication ===");
        
        wifi_init();
        esp_err_t wifi_ret = wifi_connect(SSID, PASSWORD);
        if (wifi_ret == ESP_OK) {
            wifi_connected = true;
            strncpy(ip_address, wifi_get_ip_address(), sizeof(ip_address) - 1);
            ESP_LOGI(TAG, "WiFi Connected, IP: %s", ip_address);
        } else {
            wifi_connected = false;
            ESP_LOGW(TAG, "WiFi Connection Failed");
        }
        
        init_mqtt();
        vTaskDelay(pdMS_TO_TICKS(500));
        
        send_sensor_value(&sensor_data, last_reset_code);
        
        if (inference_result == ESP_OK) {
            expected_message = 2;
            send_inference_value(&prediction);
        } else {
            expected_message = 1;
        }
        
        TickType_t wait_time = pdMS_TO_TICKS(10000);
        while (sent_message < expected_message && wait_time > 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            wait_time -= pdMS_TO_TICKS(100);
        }
        
        if (sent_message >= expected_message) {
            ESP_LOGI(TAG, "All data sent successfully");
        } else {
            ESP_LOGW(TAG, "MQTT send timeout");
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        if (client) {
            esp_mqtt_client_stop(client);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        wifi_disconnect();
        deinit_wifi();

        power_manager_save_data(&prediction, &sensor_data, wifi_connected, ip_address);
        
        power_manager_update_inference_time();
        
        ESP_LOGI(TAG, "Inference completed successfully");
        ESP_LOGI(TAG, "Tensor usage: %d bytes", prediction.tensor_usage);
    }
    
    ESP_LOGI(TAG, "=== Entering Interactive Mode ===");
    
    display_on();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    if (inference_result == ESP_OK) {
        display_show_page(current_page, &prediction, &sensor_data);
    } else {
        display_show_network_status(wifi_connected, ip_address, size);
    }
    
    button_reset_inactivity_timer();
    
    bool should_sleep = false;
    
    while (!should_sleep) {
        button_event_t event = button_poll();
        
        switch (event) {
            case BTN_EVENT_NEXT_PRESSED:
                // Next page
                current_page++;
                if (current_page > DISPLAY_PAGE_MAX) {
                    current_page = 0;
                }
                display_show_page(current_page, &prediction, &sensor_data);
                network_status_displayed = false;
                sleep_warning_displayed = false;
                break;
                
            case BTN_EVENT_PREV_PRESSED:
                if (current_page == 0) {
                    current_page = DISPLAY_PAGE_MAX;
                } else {
                    current_page--;
                }
                display_show_page(current_page, &prediction, &sensor_data);
                network_status_displayed = false;
                sleep_warning_displayed = false;
                break;
                
            case BTN_EVENT_BOTH_PRESSED:
                display_show_network_status(wifi_connected, ip_address, size);
                network_status_displayed = true;
                sleep_warning_displayed = false;
                break;
                
            case BTN_EVENT_BOTH_LONG_PRESS:
                if (!sleep_warning_displayed) {
                    display_show_sleep_warning();
                    sleep_warning_displayed = true;
                }
                break;
                
            case BTN_EVENT_BOTH_RELEASED_AFTER_LONG:
                ESP_LOGI(TAG, "User triggered sleep via long press");
                should_sleep = true;
                break;
                
            default:
                break;
        }
        
        if (button_is_inactive_for(INACTIVITY_TIMEOUT_MS)) {
            ESP_LOGI(TAG, "Inactivity timeout reached, going to sleep");
            should_sleep = true;
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    ESP_LOGI(TAG, "=== Preparing for sleep ===");
    
    power_manager_set_display_page(current_page);
    
    display_off();
    
    ESP_LOGI(TAG, "Entering deep sleep...");
    power_manager_enter_deep_sleep();
}
