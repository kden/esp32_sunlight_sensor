"""
dynamic_envs.py

Creates dynamic PlatformIO environments based on the SENSOR_ENV variable.
Enables credentials to be loaded to firmware on a per-sensor basis.

Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
Developed with assistance from ChatGPT 4o (2025) and Google Gemini 2.5 Pro (2025).
Apache 2.0 Licensed as described in the file LICENSE
"""
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

except configparser.NoOptionError as e:
    print(f"Error: Missing configuration in section '[{sensor_env}]'. {e}")
    env.Exit(1)

# Create a header file instead of using build flags
# This avoids shell escaping issues with special characters
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