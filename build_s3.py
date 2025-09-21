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





def build_and_upload(sensor_id):
    """
    Build and upload firmware for the specified sensor.

    Args:
        sensor_id (str): The sensor environment name

    Returns:
        bool: True if build and upload succeeded, False otherwise
    """
    command = f"SENSOR_ENV={sensor_id} pio run -e dynamic_sensor -t upload"
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


    # Build and upload firmware
    if build_and_upload(sensor_id):
        print(f"\n[SUCCESS] Build and upload completed successfully for {sensor_id}")
    else:
        print(f"\n[ERROR] Build and upload failed for {sensor_id}")
        sys.exit(1)


if __name__ == "__main__":
    main()