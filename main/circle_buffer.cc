#include <stdio.h>
#include "circle_buffer.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "data_structure.h"
#include "esp_log.h"

#define USER_LED_PIN GPIO_NUM_21


#define MAX_BUFFER 24

int RTC_DATA_ATTR size;
int RTC_DATA_ATTR buffer_index;
static RTC_DATA_ATTR t_bme280_s_val sensor_buffer[24];

RTC_DATA_ATTR bool data_valid_flag;
int last_reset_code;

void init_circular_buffer() {
    esp_reset_reason_t reason = esp_reset_reason();
    
    last_reset_code = (int)reason;

    gpio_reset_pin(USER_LED_PIN);
    gpio_set_direction(USER_LED_PIN, GPIO_MODE_OUTPUT);

    switch (reason) {
        case ESP_RST_POWERON:
            printf("SYSTEM: Power On Reset. Initializing Buffer...\n");
            size = 0;
            buffer_index = 0;
            data_valid_flag = true;
            
            gpio_set_level(USER_LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(100)); 
            gpio_set_level(USER_LED_PIN, 1); 
            break;

        case ESP_RST_DEEPSLEEP:
            printf("SYSTEM: Wakeup from Deep Sleep. Buffer Preserved. Size: %d\n", size);
            gpio_set_level(USER_LED_PIN, 0); 
            vTaskDelay(pdMS_TO_TICKS(100));  
            gpio_set_level(USER_LED_PIN, 1); 
            break;

        case ESP_RST_BROWNOUT: {
            printf("WARNING: Brownout Reset Detected! Check Power Supply.\n");
            gpio_set_level(USER_LED_PIN, 1); 
            vTaskDelay(pdMS_TO_TICKS(100)); 
            gpio_set_level(USER_LED_PIN, 0); 
            
            uint64_t brownout_sleep_us = 60ULL * 1000000ULL; // 1 minute
            esp_sleep_enable_timer_wakeup(brownout_sleep_us);
            esp_deep_sleep_start();
            break;
        }

        case ESP_RST_PANIC:
        case ESP_RST_WDT:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_SW:
            
            printf("WARNING: System Crash/Reset Detected (Reason: %d). Recovering...\n", reason);
            for(int i=0; i<3; i++) {
                gpio_set_level(USER_LED_PIN, 0); // ON
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(USER_LED_PIN, 1); // OFF
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            break;

        case ESP_RST_UNKNOWN:
        case ESP_RST_EXT:
        case ESP_RST_SDIO:
        case ESP_RST_USB:
        case ESP_RST_JTAG:
        case ESP_RST_EFUSE:
        case ESP_RST_PWR_GLITCH:
        case ESP_RST_CPU_LOCKUP:
        default:
            printf("SYSTEM: Unknown Reset Reason (%d)\n", reason);
            break;
    }
}


void push_data_into_stack(t_bme280_s_val data){
    sensor_buffer[buffer_index] = data;
    if(size < MAX_BUFFER){
        size += 1;
        printf("%d \n", size);
    }
    buffer_index = (buffer_index+1) % MAX_BUFFER;
}

void yield_data(t_bme280_s_val sensor_data[]) {
    int j = buffer_index;
    for (int i = 0; i < MAX_BUFFER; i++) {
        sensor_data[i] = sensor_buffer[j];
        j = (j + 1) % MAX_BUFFER;
    }
}





