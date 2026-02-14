    #pragma once

    #ifndef DATA_STRUCTURE_H
    #define DATA_STRUCTURE_H

    typedef struct bme280_sensor_output {
        float temperature;
        float humidity;
        float pressure;
    } t_bme280_s_val;

    typedef struct inference_result{
        long long int time;
        int tensor_usage;
        t_bme280_s_val predicted_data[6];
    }t_infered;

    #endif // BME280_SENSOR_OUTPUT_H
