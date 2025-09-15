import os
import configparser

# Import the 'env' environment from the PlatformIO script context
Import("env")

# Get the SENSOR_ENV from the shell's environment variables.
sensor_env = os.getenv("SENSOR_ENV", "sensor_1")

if not sensor_env:
    print("SENSOR_ENV not set. Skipping dynamic configuration.")
    env.Exit(0)

config_file = "credentials.ini"

if not os.path.exists(config_file):
    print(f"Error: Credentials file '{config_file}' not found.")
    env.Exit(1)

print(f"Loading credentials for '{sensor_env}' from {config_file}")

config = configparser.ConfigParser()
config.read(config_file)

# Check for the required sections
if not config.has_section("all_sensors"):
    print(f"Error: Section '[all_sensors]' not found in {config_file}.")
    env.Exit(1)

if not config.has_section(sensor_env):
    print(f"Error: Section '[{sensor_env}]' not found in {config_file}.")
    env.Exit(1)

try:
    url = config.get("all_sensors", "url")
    sensor_set_id = config.get(sensor_env, "sensor_set_id")
    sensor_id = config.get(sensor_env, "sensor_id")
    bearer_token = config.get(sensor_env, "bearer_token")
    wifi_cred = config.get(sensor_env, "wifi_credentials")
    sda_gpio = config.get(sensor_env, "sda_gpio")
    scl_gpio = config.get(sensor_env, "scl_gpio")
    battery_adc_gpio = config.get(sensor_env, "battery_adc_gpio", fallback="2")
    night_start_hour = config.get(sensor_env, "night_start_hour", fallback="22")
    night_end_hour = config.get(sensor_env, "night_end_hour", fallback="4")
    local_timezone = config.get(sensor_env, "local_timezone", fallback="CST6CDT,M3.2.0,M11.1.0")

except configparser.NoOptionError as e:
    print(f"Error: Missing configuration in section '[{sensor_env}]'. {e}")
    env.Exit(1)

# Create a header file instead of using build flags
header_content = f'''/**
 * Auto-generated configuration file
 * DO NOT EDIT MANUALLY
 */
#pragma once

#define CONFIG_SENSOR_ID "{sensor_id}"
#define CONFIG_BEARER_TOKEN "{bearer_token}"
#define CONFIG_WIFI_CREDENTIALS "{wifi_cred}"
#define CONFIG_API_URL "{url}"
#define CONFIG_SENSOR_SET "{sensor_set_id}"
#define CONFIG_SENSOR_SDA_GPIO {sda_gpio}
#define CONFIG_SENSOR_SCL_GPIO {scl_gpio}
#define CONFIG_BATTERY_ADC_GPIO {battery_adc_gpio}
#define CONFIG_NIGHT_START_HOUR {night_start_hour}
#define CONFIG_NIGHT_END_HOUR {night_end_hour}
#define CONFIG_LOCAL_TIMEZONE "{local_timezone}"
'''

# Write the header file
with open("include/generated_config.h", "w") as f:
    f.write(header_content)

print("Successfully generated configuration header:")
print(f"  - SENSOR_ID: {sensor_id}")
print(f"  - BEARER_TOKEN: redacted")
print(f"  - WIFI_CREDENTIALS: redacted")
print(f"  - API_URL: {url}")
print(f"  - SENSOR_SET: {sensor_set_id}")
print(f"  - SDA_GPIO: {sda_gpio}")
print(f"  - SCL_GPIO: {scl_gpio}")
print(f"  - BATTERY_ADC_GPIO: {battery_adc_gpio}")
print(f"  - NIGHT_START_HOUR: {night_start_hour}")
print(f"  - NIGHT_END_HOUR: {night_end_hour}")
print(f"  - LOCAL_TIMEZONE: {local_timezone}")