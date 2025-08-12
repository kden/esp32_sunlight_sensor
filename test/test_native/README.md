# Instructions for running the task_send_data unit tests

## Setup

1. Place the test_task_send_data.c file in: `test/test_native/`

2. Your existing platformio.ini already has the correct native environment configuration:
   ```ini
   [env:native]
   platform = native
   framework =
   build_flags = -D TEST
   lib_deps =
       throwtheswitch/Unity
   # Override the source directory for native tests
   src_dir = test/test_native
   ```

   No changes needed to platformio.ini - it's already correctly configured!

3. Create a unity_config.h file in test/test_native/ to configure Unity (optional):
   ```c
   #ifndef UNITY_CONFIG_H
   #define UNITY_CONFIG_H
   
   #define UNITY_INCLUDE_DOUBLE
   #define UNITY_DOUBLE_PRECISION 1e-12
   #define UNITY_INCLUDE_FLOAT
   #define UNITY_FLOAT_PRECISION 1e-6
   
   #endif
   ```

## Running the tests

From your project root directory:

```bash
# Run all native tests
pio test -e native

# Run only this specific test
pio test -e native -f test_task_send_data

# Run with verbose output
pio test -e native -v
```

## Integration with actual source code

To test the actual implementation instead of the mock version:

1. Modify task_send_data.h to expose the test function:
   ```c
   #ifdef TEST
   void process_data_send_cycle(app_context_t *context, time_t now, time_t *last_ntp_sync_time);
   #endif
   ```

2. In task_send_data.c, compile the function for testing:
   ```c
   #ifdef TEST
   // Make internal functions available for testing
   void process_data_send_cycle(app_context_t *context, time_t now, time_t *last_ntp_sync_time) {
       // ... existing implementation
   }
   #endif
   ```

3. Create a mock header file (test/test_native/mocks/esp_mocks.h) for ESP-IDF functions.

4. Link the actual source in the test build flags:
   ```ini
   build_flags = 
       -D TEST 
       -I src/
       -I components/
   ```

## Expected output

When tests pass, you should see:
```
test_wifi_unavailable_stores_readings:PASS
test_wifi_available_sends_stored_and_new_readings:PASS
test_send_failure_stores_new_readings:PASS
test_buffer_overflow_drops_oldest_readings:PASS
test_no_readings_to_send:PASS
test_partial_send_failure_with_stored_readings:PASS

6 Tests 0 Failures 0 Ignored
```