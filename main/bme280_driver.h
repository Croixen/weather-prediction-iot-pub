#ifndef READING_SENSOR_H_
#define READING_SENSOR_H_
#include "data_structure.h"
#include "i2c_bus.h"

void initialize_i2c(void);
void read_sensor(t_bme280_s_val* payload);

// Get the shared I2C bus handle
i2c_bus_handle_t get_i2c_bus_handle(void);

#endif

