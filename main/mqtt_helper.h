#ifndef SEND_MQTT_H_
#define SEND_MQTT_H_

    #include "data_structure.h"
    #include "mqtt_client.h"
    
    extern esp_mqtt_client_handle_t client;
    extern int expected_message;
    extern int sent_message;
    
    void init_mqtt();
    void log_error_if_nonzero(const char *message, int error_code);
    void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
    void send_sensor_value(t_bme280_s_val* payload, int status_code);
    void send_inference_value(t_infered* payload);

#endif

