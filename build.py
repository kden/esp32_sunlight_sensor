#!/usr/bin/env python3
"""
ESP32 Sensor Build and Upload Script

This script sets up the build environment and compiles/uploads firmware
for ESP32-based sensor devices. It handles different board types and
automatically configures the appropriate SDK settings.

Usage:
    python build_sensor.py <sensor_id> [clean]

Examples:
    python build_sensor.py sensor_1
    python build_sensor.py sensor_temp clean
"""

import sys
import os
import subprocess
import configparser
import shutil
from pathlib import Path


def print_usage():
    """Print usage instructions and exit."""
    print("Usage: python build_sensor.py <sensor_id> [clean]")
    print("\nExamples:")
    print("  python build_sensor.py sensor_1")
    print("  python build_sensor.py sensor_temp clean")
    print("\nThe 'clean' parameter will remove build artifacts before building.")
    sys.exit(1)


def run_command(command, description):
    """
    Run a shell command and handle errors.

    Args:
        command (str): The command to execute
        description (str): Description of what the command does

    Returns:
        bool: True if command succeeded, False otherwise
    """
    print(f"\n[INFO] {description}")
    print(f"[CMD]  {command}")

    try:
        result = subprocess.run(command, shell=True, check=True,
                                capture_output=True, text=True)
        if result.stdout:
            print(result.stdout)
        return True
    except subprocess.CalledProcessError as e:
        print(f"[ERROR] Command failed with return code {e.returncode}")
        if e.stdout:
            print(f"STDOUT: {e.stdout}")
        if e.stderr:
            print(f"STDERR: {e.stderr}")
        return False


def clean_build_artifacts(sensor_id):
    """
    Clean build artifacts for the specified sensor.

    Args:
        sensor_id (str): The sensor environment name
    """
    print(f"\n[INFO] Cleaning build artifacts for {sensor_id}")

    # Clean PlatformIO build
    if not run_command(f"SENSOR_ENV={sensor_id} pio run -e {sensor_id} -t clean",
                       "Cleaning PlatformIO build"):
        print("[WARN] PlatformIO clean command failed, continuing...")

    # Remove build directories
    dirs_to_remove = ["build", ".pio/build"]
    for dir_path in dirs_to_remove:
        if os.path.exists(dir_path):
            print(f"[INFO] Removing directory: {dir_path}")
            try:
                shutil.rmtree(dir_path)
            except Exception as e:
                print(f"[WARN] Failed to remove {dir_path}: {e}")

    # Remove SDK config file
    sdkconfig_file = f"sdkconfig.{sensor_id}"
    if os.path.exists(sdkconfig_file):
        print(f"[INFO] Removing SDK config: {sdkconfig_file}")
        try:
            os.remove(sdkconfig_file)
        except Exception as e:
            print(f"[WARN] Failed to remove {sdkconfig_file}: {e}")


def parse_platformio_ini(sensor_id):
    """
    Parse platformio.ini to determine which base environment the sensor extends.

    Args:
        sensor_id (str): The sensor environment name

    Returns:
        str: The base environment name (esp32s3_base or esp32c3_base)

    Raises:
        SystemExit: If parsing fails or sensor environment not found
    """
    platformio_ini = "platformio.ini"

    if not os.path.exists(platformio_ini):
        print(f"[ERROR] {platformio_ini} not found in current directory")
        sys.exit(1)

    config = configparser.ConfigParser()
    try:
        config.read(platformio_ini)
    except Exception as e:
        print(f"[ERROR] Failed to parse {platformio_ini}: {e}")
        sys.exit(1)

    env_section = f"env:{sensor_id}"
    if not config.has_section(env_section):
        print(f"[ERROR] Environment section '[{env_section}]' not found in {platformio_ini}")
        print(f"[INFO] Available environments:")
        for section in config.sections():
            if section.startswith("env:"):
                print(f"  - {section}")
        sys.exit(1)

    if not config.has_option(env_section, "extends"):
        print(f"[ERROR] No 'extends' option found in section '[{env_section}]'")
        sys.exit(1)

    extends = config.get(env_section, "extends")
    print(f"[INFO] Environment '{sensor_id}' extends '{extends}'")

    # Map the extends value to our expected base environments
    if extends == "env:esp32s3_base":
        return "esp32s3_base"
    elif extends == "env:esp32c3_base":
        return "esp32c3_base"
    else:
        print(f"[ERROR] Unknown base environment: {extends}")
        print(f"[INFO] Expected 'env:esp32s3_base' or 'env:esp32c3_base'")
        sys.exit(1)


def setup_sdkconfig(base_env):
    """
    Copy the appropriate SDK config defaults file based on the base environment.

    Args:
        base_env (str): The base environment (esp32s3_base or esp32c3_base)
    """
    source_file = f"sdkconfig.defaults_{base_env}"
    target_file = "sdkconfig.defaults"

    if not os.path.exists(source_file):
        print(f"[ERROR] Source SDK config file not found: {source_file}")
        print("[INFO] Expected files:")
        print("  - sdkconfig.defaults_esp32s3_base")
        print("  - sdkconfig.defaults_esp32c3_base")
        sys.exit(1)

    print(f"[INFO] Copying {source_file} to {target_file}")
    try:
        shutil.copy2(source_file, target_file)
        print(f"[INFO] SDK config setup complete for {base_env}")
    except Exception as e:
        print(f"[ERROR] Failed to copy SDK config: {e}")
        sys.exit(1)


def build_and_upload(sensor_id):
    """
    Build and upload firmware for the specified sensor.

    Args:
        sensor_id (str): The sensor environment name

    Returns:
        bool: True if build and upload succeeded, False otherwise
    """
    command = f"SENSOR_ENV={sensor_id} pio run -e {sensor_id} -t upload"
    return run_command(command, f"Building and uploading firmware for {sensor_id}")


def main():
    """Main script entry point."""
    # Check command line arguments
    if len(sys.argv) < 2 or len(sys.argv) > 3:
        print_usage()

    sensor_id = sys.argv[1]
    should_clean = len(sys.argv) == 3 and sys.argv[2].lower() == "clean"

    print(f"ESP32 Sensor Build Script")
    print(f"========================")
    print(f"Sensor ID: {sensor_id}")
    print(f"Clean build: {'Yes' if should_clean else 'No'}")

    # Verify we're in the correct directory
    if not os.path.exists("platformio.ini"):
        print("[ERROR] platformio.ini not found. Please run this script from the project root directory.")
        sys.exit(1)

    # Clean build artifacts if requested
    if should_clean:
        clean_build_artifacts(sensor_id)

    # Parse platformio.ini to determine base environment
    base_env = parse_platformio_ini(sensor_id)

    # Setup SDK config for the appropriate board type
    setup_sdkconfig(base_env)

    # Build and upload firmware
    if build_and_upload(sensor_id):
        print(f"\n[SUCCESS] Build and upload completed successfully for {sensor_id}")
    else:
        print(f"\n[ERROR] Build and upload failed for {sensor_id}")
        sys.exit(1)


if __name__ == "__main__":
    main()