#include "mqtt_helper.h"
#include <inttypes.h>
#include "cJSON.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "data_structure.h"

#define TAG "MQTT"

extern const uint8_t emqx_cert_pem_start[] asm("_binary_emqxsl_ca_crt_start");
extern const uint8_t emqx_cert_pem_end[]   asm("_binary_emqxsl_ca_crt_end");

esp_mqtt_client_handle_t client = NULL;
cJSON* json = NULL;
int expected_message = 1; 
int sent_message = 0;

//Checkking error log 
void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

//Handling mqtt_event_handler by looking at the event id, and do something about it
void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data){
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data; //note: in Cpp, you have to casting, idk why, but what i know is void* typically is like "any" type in TS
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
    
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            sent_message++;
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
                ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            }
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
        }
}

void init_mqtt(){
    char* URL = "mqtts://BROKER_URL:8883";
    esp_mqtt_client_config_t mqtt_cfg{};
    mqtt_cfg.broker.address.uri = URL;
    mqtt_cfg.broker.verification.certificate = (const char *)emqx_cert_pem_start;
    mqtt_cfg.session.keepalive = 60;
    mqtt_cfg.credentials.authentication.password = "REDACTED";
    mqtt_cfg.credentials.username = "REDACTED";
    mqtt_cfg.credentials.client_id = "ESP32S3-NODE";
    mqtt_cfg.network.disable_auto_reconnect = false;
    mqtt_cfg.network.reconnect_timeout_ms = 5000;

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void send_sensor_value(t_bme280_s_val* payload, int status_code){
    if (client == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized, cannot send sensor value");
        return;
    }
    
    json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "Temperature", payload -> temperature);
    cJSON_AddNumberToObject(json, "Humidity", payload -> humidity);
    cJSON_AddNumberToObject(json, "Pressure", payload -> pressure);
    cJSON_AddNumberToObject(json, "Health", status_code);


    char* json_string = cJSON_PrintUnformatted(json);

    if(json_string){
        esp_mqtt_client_publish(client, "/sensor", json_string, 0, 1, 0);
    }else{
        ESP_LOGE(TAG, "Failed to Create JSON String");
    }
    
    cJSON_Delete(json);
    free(json_string);
}

void send_inference_value(t_infered* payload){
    if (client == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized, cannot send inference value");
        return;
    }
    
    json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "tensor_arena", payload -> tensor_usage);

    cJSON_AddNumberToObject(json, "time", payload -> time);
    cJSON* pred_temperature = cJSON_AddArrayToObject(json, "pred_temp");
    cJSON* pred_humidity = cJSON_AddArrayToObject(json, "pred_humidity");
    cJSON* pred_pressure = cJSON_AddArrayToObject(json, "pred_pressure");

    for (int i = 0; i < 6; i++){
        cJSON_AddItemToArray(pred_temperature, cJSON_CreateNumber(payload->predicted_data[i].temperature));
        cJSON_AddItemToArray(pred_humidity, cJSON_CreateNumber(payload->predicted_data[i].humidity));
        cJSON_AddItemToArray(pred_pressure, cJSON_CreateNumber(payload->predicted_data[i].pressure));
    }

    char* json_string = cJSON_PrintUnformatted(json);

    if(json_string){
        esp_mqtt_client_publish(client, "/predicted", json_string, 0, 1, 0);
    }else{
        ESP_LOGE(TAG, "Failed to Create JSON String");
    }

    cJSON_Delete(json);
    free(json_string);
}


