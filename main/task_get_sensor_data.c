

#include "task_get_sensor_data.h"
void task_get_sensor_data(void *arg) {

    // Every 15 seconds
    // Get value of light sensor
    get_ambient_light(light_sensor_hdl, &lux);

    // Get timestamp
    // Create mutex lock on shared list of readings
    // If there is no list, you may have to allocate memory for list
    // Append new reading to end of list
    // Release mutex lock on shared list of readings

}