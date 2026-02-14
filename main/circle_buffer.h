#pragma once

#ifndef CIRCLE_BUFFER_H
#define CIRCLE_BUFFER_H
    #include "data_structure.h"


    extern int size;
    extern int buffer_index;
    extern int last_reset_code;

    // void preload_sensor_buffer();
    void init_circular_buffer();
    void push_data_into_stack(t_bme280_s_val data);
    void yield_data(t_bme280_s_val sensor_data[]);
    
#endif