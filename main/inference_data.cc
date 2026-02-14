#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include <esp_heap_caps.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#include "model_data.h"
#include "inference_data.h"
#include "data_structure.h"
#include "circle_buffer.h"

 // tflite::MicroMutableOpResolver<13> resolver;
 // constexpr int scratchBufSize = 60 * 1024;
 // constexpr int kTensorArenaSize = 150 * 1024 + scratchBufSize;



namespace {
    const tflite::Model* model = nullptr;
    tflite::MicroInterpreter* interpreter = nullptr;
    TfLiteTensor* input = nullptr;
    TfLiteTensor* output = nullptr;
    #if CONFIG_NN_OPTIMIZED
    constexpr int scratchBufSize = 110 * 1024;
    #else
    constexpr int scratchBufSize = 0;
    #endif
    constexpr int kTensorArenaSize = 180 * 1024 + scratchBufSize;
    static uint8_t *tensor_arena;
} 

t_infered prediction_result;

void min_max_scaler(t_bme280_s_val* payload){
  float min_temp = 22;
  float min_humidity = 24;
  float min_pressure = 1001.3;
  float max_temp = 38;
  float max_humidity = 100;
  float max_pressure = 1013.9;
  
  bool clamped = false;
  
  payload->temperature = (payload->temperature - min_temp) / (max_temp - min_temp);
  payload->humidity = (payload->humidity - min_humidity) / (max_humidity - min_humidity);
  payload->pressure = (payload->pressure - min_pressure) / (max_pressure - min_pressure);

  if(payload->temperature > 1) {
    payload->temperature = 1;
    clamped = true;
    MicroPrintf("WARNING: Temperature clamped to max");
  }
  if(payload->temperature < 0) {
    payload->temperature = 0;
    clamped = true;
    MicroPrintf("WARNING: Temperature clamped to min");
  }

  if(payload->humidity > 1) {
    payload->humidity = 1;
    clamped = true;
    MicroPrintf("WARNING: Humidity clamped to max");
  }
  if(payload->humidity < 0) {
    payload->humidity = 0;
    clamped = true;
    MicroPrintf("WARNING: Humidity clamped to min");
  }

  if(payload->pressure > 1) {
    payload->pressure = 1;
    clamped = true;
    MicroPrintf("WARNING: Pressure clamped to max");
  }
  if(payload->pressure < 0) {
    payload->pressure = 0;
    clamped = true;
    MicroPrintf("WARNING: Pressure clamped to min");
  }
  
  if (clamped) {
    MicroPrintf("Sensor values out of expected range - possible malfunction");
  }
}

float inv_min_max(float scaled, float min, float max) {
  return scaled * (max - min) + min;
}

void init_interpeter(){
  model = tflite::GetModel(i8Quantized_tflite);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    MicroPrintf("Model provided is schema version %d not equal to supported "
                "version %d.", model->version(), TFLITE_SCHEMA_VERSION);
    return;
  }

  if (tensor_arena == NULL) {
    tensor_arena = (uint8_t *) \
    heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }

  if (tensor_arena == NULL) {
    printf("Couldn't allocate memory of %d bytes\n", kTensorArenaSize);
    return;
  }

  static tflite::MicroMutableOpResolver<15> resolver;

  resolver.AddAdd();
  resolver.AddDequantize(); 
  resolver.AddQuantize();
  resolver.AddFill();
  resolver.AddFullyConnected();
  resolver.AddLogistic();
  resolver.AddMul();
  resolver.AddPack();
  resolver.AddReshape();
  resolver.AddShape();
  resolver.AddSplit();
  resolver.AddStridedSlice();
  resolver.AddTanh();
  resolver.AddTranspose();
  resolver.AddUnpack();

  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize);
  interpreter = &static_interpreter;

  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    MicroPrintf("AllocateTensors() failed");
    return;
  }

  input = interpreter->input(0);
  output = interpreter->output(0);

  return;
}

esp_err_t inference_invoke(){

  if (interpreter == nullptr) {
    MicroPrintf("Interpreter not initialized!");
    return ESP_FAIL;
  }

  t_bme280_s_val sensor_data[24];
  yield_data(sensor_data);

  TfLiteTensor* input = interpreter->input(0);
  float* input_data = input->data.f;

  for (int i = 0; i < 24; i++) {

      min_max_scaler(&sensor_data[i]);

      input_data[i * 3 + 0] = sensor_data[i].temperature;
      input_data[i * 3 + 1] = sensor_data[i].humidity;
      input_data[i * 3 + 2] = sensor_data[i].pressure;
  }

  int64_t start_time = esp_timer_get_time();
  TfLiteStatus invoke_status = interpreter->Invoke();
  int64_t end_time = esp_timer_get_time();

  if (invoke_status != kTfLiteOk) {
      MicroPrintf("Invoke failed");
      return ESP_FAIL;
  } else {
      MicroPrintf("Invoke done");
      prediction_result.time = (end_time - start_time);
      prediction_result.tensor_usage = interpreter->arena_used_bytes();
  }

  TfLiteTensor* y = interpreter->output(0);
  float* y_data = y->data.f;

  int steps = y->dims->data[1];
  int feats = y->dims->data[2];

  for (int t = 0; t < steps; t++) {
      for (int f = 0; f < feats; f++) {

          int index = t * feats + f;
          float f_val = y_data[index];    

          if (f == 0){
              f_val = inv_min_max(f_val, 22, 38);
              prediction_result.predicted_data[t].temperature = f_val;
          } 
          if (f == 1){
              f_val = inv_min_max(f_val, 24, 100);
              prediction_result.predicted_data[t].humidity = f_val;
          } 
          if (f == 2){
              f_val = inv_min_max(f_val, 1001.3, 1013.9);
              prediction_result.predicted_data[t].pressure = f_val;
          }
      }
  }

  return ESP_OK;
}