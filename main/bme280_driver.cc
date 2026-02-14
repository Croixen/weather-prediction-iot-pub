#include "bme280_driver.h"

#include "data_structure.h"

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bme280.h"
#include "i2c_bus.h"


static i2c_bus_handle_t bus_handler = NULL;
static bme280_handle_t bme280 = NULL;

#define TAG "[I2C]"

i2c_bus_handle_t get_i2c_bus_handle(void) {
    return bus_handler;
}

void initialize_i2c(){
    ESP_LOGI(TAG, "OPENING I2C PROTOCOL");
    
    i2c_config_t i2c_conf;
    memset(&i2c_conf, 0, sizeof(i2c_config_t));
    
    i2c_conf.mode = I2C_MODE_MASTER;
    i2c_conf.sda_io_num = GPIO_NUM_5;
    i2c_conf.scl_io_num = GPIO_NUM_6;
    i2c_conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_conf.master.clk_speed = 100000;

    bus_handler = i2c_bus_create(I2C_NUM_0, &i2c_conf);
    if(bus_handler == NULL){
        ESP_LOGE(TAG, "Failed to init i2c\n");
    }
    bme280 = bme280_create(bus_handler, 0x76);
    bme280_default_init(bme280);
}

void read_sensor(t_bme280_s_val* payload) {
    int retries  = 0;
    esp_err_t read_result = ESP_FAIL;
    
    while(retries < 10){
        read_result = bme280_read_humidity(bme280, &(payload->humidity));
        if(read_result == ESP_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        retries++;
    }

    if(retries == 10){
        ESP_LOGE(TAG, "The I2C Initialization Failed after 10 retries!");
        payload->humidity = -1.0f;
        payload->temperature = -1.0f;
        payload->pressure = -1.0f;
        return;
    }

    bme280_read_temperature(bme280, &(payload->temperature));
    bme280_read_pressure(bme280, &(payload->pressure));
}