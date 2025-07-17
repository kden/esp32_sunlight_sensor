
#include "task_send_data.h"

#include "http.h"

void task_send_data(void *arg) {


    // Start loop
    // If 5 minutes has passed since last data send
    // Power up wifi and connect
    // If an hour has passed since last NTP sync
    // Then Sync our clock with NTP (only if wifi connection is successfully made)
    initialize_sntp();


    // Create mutex lock on shared list of readings
    // Retrieve list of readings
    // Send data via wifi
    send_sensor_data(reading_buffer, reading_idx, CONFIG_SENSOR_ID, CONFIG_BEARER_TOKEN);

    // Delete contents of shared list of readings
    // Release mutex lock on shared list of readings
    // End of loop

}




