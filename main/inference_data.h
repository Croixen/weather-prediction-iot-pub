#pragma once

#ifndef INFERENCE_DATA_H
#define INFERENCE_DATA_H

    #include "data_structure.h"
    #include "esp_err.h"

    extern t_infered prediction_result;
    void min_max_scaler(t_bme280_s_val* payload);
    float inv_min_max(float scaled, float min, float max);
    void init_interpeter();
    esp_err_t inference_invoke();
#endif
