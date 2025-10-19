#!/usr/bin/env python3
"""
ESP32 Sensor Build and Upload Script

This script sets up the build environment and compiles/uploads firmware
for ESP32-based sensor devices. It handles different board types and
automatically configures the appropriate SDK settings.

Usage:
    python build_sensor.py <sensor_id> [clean] [upload]

Examples:
    python build_sensor.py sensor_1
    python build_sensor.py sensor_temp clean
    python build_sensor.py sensor_1 upload
    python build_sensor.py sensor_temp clean upload
"""

import sys
import os
import subprocess
import configparser
import shutil
from pathlib import Path


def print_usage():
    """Print usage instructions and exit."""
    print("Usage: python build_sensor.py <sensor_id> [clean] [upload]")
    print("\nExamples:")
    print("  python build_sensor.py sensor_1")
    print("  python build_sensor.py sensor_temp clean")
    print("  python build_sensor.py sensor_1 upload")
    print("  python build_sensor.py sensor_temp clean upload")
    print("\nOptions:")
    print("  clean  - Remove build artifacts before building")
    print("  upload - Upload firmware after building (default: build only)")
    sys.exit(1)


def run_command(command, description, capture_output=False):
    """
    Run a shell command and handle errors.

    Args:
        command (str): The command to execute
        description (str): Description of what the command does
        capture_output (bool): If True, capture output; if False, stream to terminal

    Returns:
        bool: True if command succeeded, False otherwise
    """
    print(f"\n{'='*60}")
    print(f"[INFO] {description}")
    print(f"[CMD]  {command}")
    print('='*60)

    try:
        if capture_output:
            # Capture output for quiet operations (like clean)
            result = subprocess.run(command, shell=True, check=True,
                                    capture_output=True, text=True)
            if result.stdout:
                print(result.stdout)
        else:
            # Stream output in real-time for build operations
            result = subprocess.run(command, shell=True, check=True)

        return True
    except subprocess.CalledProcessError as e:
        print(f"\n[ERROR] Command failed with return code {e.returncode}")
        if capture_output and hasattr(e, 'stdout') and e.stdout:
            print(f"STDOUT: {e.stdout}")
        if capture_output and hasattr(e, 'stderr') and e.stderr:
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
    if not run_command(f"SENSOR_ENV={sensor_id} pio run -e dynamic_sensor -t clean",
                       "Cleaning PlatformIO build", capture_output=True):
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


def build_firmware(sensor_id):
    """
    Build firmware for the specified sensor (without uploading).

    Args:
        sensor_id (str): The sensor environment name

    Returns:
        bool: True if build succeeded, False otherwise
    """
    command = f"SENSOR_ENV={sensor_id} pio run -e dynamic_sensor"
    return run_command(command, f"Building firmware for {sensor_id}", capture_output=False)


def upload_firmware(sensor_id):
    """
    Upload firmware for the specified sensor (assumes already built).

    Args:
        sensor_id (str): The sensor environment name

    Returns:
        bool: True if upload succeeded, False otherwise
    """
    command = f"SENSOR_ENV={sensor_id} pio run -e dynamic_sensor -t upload"
    return run_command(command, f"Uploading firmware for {sensor_id}", capture_output=False)


def main():
    """Main script entry point."""
    # Check command line arguments
    if len(sys.argv) < 2:
        print_usage()

    sensor_id = sys.argv[1]

    # Parse optional arguments
    should_clean = False
    should_upload = False

    for arg in sys.argv[2:]:
        arg_lower = arg.lower()
        if arg_lower == "clean":
            should_clean = True
        elif arg_lower == "upload":
            should_upload = True
        else:
            print(f"[ERROR] Unknown argument: {arg}")
            print_usage()

    print("="*60)
    print("ESP32 Sensor Build Script")
    print("="*60)
    print(f"Sensor ID:      {sensor_id}")
    print(f"Clean build:    {'Yes' if should_clean else 'No'}")
    print(f"Upload:         {'Yes' if should_upload else 'No (build only)'}")
    print("="*60)

    # Verify we're in the correct directory
    if not os.path.exists("platformio.ini"):
        print("[ERROR] platformio.ini not found. Please run this script from the project root directory.")
        sys.exit(1)

    # Clean build artifacts if requested
    if should_clean:
        clean_build_artifacts(sensor_id)

    # Build firmware
    print("\n" + "="*60)
    print("BUILDING FIRMWARE")
    print("="*60)

    if not build_firmware(sensor_id):
        print(f"\n{'='*60}")
        print(f"[ERROR] Build failed for {sensor_id}")
        print("="*60)
        sys.exit(1)

    print(f"\n{'='*60}")
    print(f"[SUCCESS] Build completed successfully for {sensor_id}")
    print("="*60)

    # Upload firmware if requested
    if should_upload:
        print("\n" + "="*60)
        print("UPLOADING FIRMWARE")
        print("="*60)

        if not upload_firmware(sensor_id):
            print(f"\n{'='*60}")
            print(f"[ERROR] Upload failed for {sensor_id}")
            print("="*60)
            sys.exit(1)

        print(f"\n{'='*60}")
        print(f"[SUCCESS] Upload completed successfully for {sensor_id}")
        print("="*60)
    else:
        print("\n[INFO] Skipping upload (use 'upload' argument to upload)")

    print(f"\n{'='*60}")
    print(f"[SUCCESS] All operations completed for {sensor_id}")
    print("="*60)


if __name__ == "__main__":
    main()