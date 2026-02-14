idf_component_register(SRCS "main.cc" "bme280_driver.cc" "circle_buffer.cc" "inference_data.cc" "mqtt_helper.cc" "wifi_helper.cc" "model_data.cc" "display_driver.cc" "button_handler.cc" "power_manager.cc"
                       INCLUDE_DIRS "."
                       REQUIRES freertos log esp_system heap nvs_flash driver bme280 i2c_bus esp_timer mqtt json esp_wifi
                       EMBED_TXTFILES "certs/emqxsl-ca.crt")
