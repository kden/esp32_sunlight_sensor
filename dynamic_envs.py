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
    # Exit cleanly if no environment is specified
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

# Optional keep-alive parameters
keepalive_gpio = None
keepalive_interval_minutes = None
keepalive_duration_seconds = None

try:
    if config.has_option(sensor_env, "keepalive_gpio"):
        keepalive_gpio = config.get(sensor_env, "keepalive_gpio")
    if config.has_option(sensor_env, "keepalive_interval_minutes"):
        keepalive_interval_minutes = config.get(sensor_env, "keepalive_interval_minutes")
    if config.has_option(sensor_env, "keepalive_duration_seconds"):
        keepalive_duration_seconds = config.get(sensor_env, "keepalive_duration_seconds")
except configparser.NoOptionError:
    # Keep-alive parameters are optional, so ignore errors
    pass

#
# Use 'env.Append(BUILD_FLAGS=...)'
# This directly adds flags to the compiler command line.
# We wrap the values in escaped quotes to handle them as strings in C.
#
build_flags = [
    f'-D CONFIG_SENSOR_ID=\\"{sensor_id}\\"',
    f'-D CONFIG_BEARER_TOKEN=\\"{bearer_token}\\"',
    f'-D CONFIG_WIFI_CREDENTIALS=\\"{wifi_cred}\\"',
    f'-D CONFIG_API_URL=\\"{url}\\"',
    f'-D CONFIG_SENSOR_SET=\\"{sensor_set_id}\\"',
    f'-D CONFIG_SENSOR_SDA_GPIO={sda_gpio}',
    f'-D CONFIG_SENSOR_SCL_GPIO={scl_gpio}'
]

# Add keep-alive configuration if present
if keepalive_gpio is not None:
    build_flags.append(f'-D CONFIG_KEEPALIVE_GPIO={keepalive_gpio}')
    print(f"  - KEEPALIVE_GPIO: {keepalive_gpio}")

if keepalive_interval_minutes is not None:
    build_flags.append(f'-D CONFIG_KEEPALIVE_INTERVAL_MINUTES={keepalive_interval_minutes}')
    print(f"  - KEEPALIVE_INTERVAL_MINUTES: {keepalive_interval_minutes}")

if keepalive_duration_seconds is not None:
    build_flags.append(f'-D CONFIG_KEEPALIVE_DURATION_SECONDS={keepalive_duration_seconds}')
    print(f"  - KEEPALIVE_DURATION_SECONDS: {keepalive_duration_seconds}")

env.Append(BUILD_FLAGS=build_flags)

# If this is run in CICD and credentials are logged, they'll be leaked.
print("Successfully applied build flags:")
print(f"  - SENSOR_ID: {sensor_id}")
print(f"  - BEARER_TOKEN: redacted")
print(f"  - WIFI_CREDENTIALS: redacted")
print(f"  - API_URL: {url}")
print(f"  - SENSOR_SET: {sensor_set_id}")
print(f"  - SDA_GPIO: {sda_gpio}")
print(f"  - SCL_GPIO: {scl_gpio}")

if keepalive_gpio is None:
    print("  - KEEPALIVE: disabled (no keepalive_gpio specified)")
else:
    print(f"  - KEEPALIVE: enabled on GPIO {keepalive_gpio}")