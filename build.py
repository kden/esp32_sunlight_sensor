#!/usr/bin/env python3
"""
ESP32-C3 Sensor Build Script

Simplified build script for ESP32-C3 only. Handles building and uploading
firmware for different sensor configurations.

Usage:
    ./build.py <sensor_id> [clean] [upload]

Examples:
    ./build.py sensor_temp             # Build only
    ./build.py sensor_temp upload      # Build and upload
    ./build.py sensor_temp clean       # Clean then build
    ./build.py sensor_temp clean upload # Clean, build, and upload

Arguments:
    sensor_id   - The sensor environment name from platformio.ini (e.g., sensor_temp)
    clean       - Optional: Clean build artifacts before building
    upload      - Optional: Upload firmware after building
"""

import sys
import os
import subprocess
import shutil


def print_usage():
    """Print usage information and exit."""
    print(__doc__)
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
    if not run_command(f"SENSOR_ENV={sensor_id} pio run -e {sensor_id} -t clean",
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


def verify_environment(sensor_id):
    """
    Verify the sensor environment exists in platformio.ini.

    Args:
        sensor_id (str): The sensor environment name

    Returns:
        bool: True if environment exists, False otherwise
    """
    platformio_ini = "platformio.ini"

    if not os.path.exists(platformio_ini):
        print(f"[ERROR] {platformio_ini} not found in current directory")
        return False

    # Simple check - look for the environment section
    with open(platformio_ini, 'r') as f:
        content = f.read()
        if f"[env:{sensor_id}]" not in content:
            print(f"[ERROR] Environment '[env:{sensor_id}]' not found in {platformio_ini}")
            print(f"[INFO] Please check your platformio.ini for available environments")
            return False

    return True


def setup_sdkconfig():
    """
    Copy the ESP32-C3 SDK config defaults file.
    """
    source_file = "sdkconfig.defaults_esp32c3_base"
    target_file = "sdkconfig.defaults"

    if not os.path.exists(source_file):
        print(f"[ERROR] Source SDK config file not found: {source_file}")
        sys.exit(1)

    print(f"[INFO] Copying {source_file} to {target_file}")
    try:
        shutil.copy2(source_file, target_file)
        print(f"[INFO] SDK config setup complete for ESP32-C3")
    except Exception as e:
        print(f"[ERROR] Failed to copy SDK config: {e}")
        sys.exit(1)


def build_firmware(sensor_id):
    """
    Build firmware for the specified sensor (without uploading).

    Args:
        sensor_id (str): The sensor environment name

    Returns:
        bool: True if build succeeded, False otherwise
    """
    command = f"SENSOR_ENV={sensor_id} pio run -e {sensor_id}"
    return run_command(command, f"Building firmware for {sensor_id}", capture_output=False)


def upload_firmware(sensor_id):
    """
    Upload firmware for the specified sensor (assumes already built).

    Args:
        sensor_id (str): The sensor environment name

    Returns:
        bool: True if upload succeeded, False otherwise
    """
    command = f"SENSOR_ENV={sensor_id} pio run -e {sensor_id} -t upload"
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
    print("ESP32-C3 Sensor Build Script")
    print("="*60)
    print(f"Sensor ID:      {sensor_id}")
    print(f"Clean build:    {'Yes' if should_clean else 'No'}")
    print(f"Upload:         {'Yes' if should_upload else 'No (build only)'}")
    print("="*60)

    # Verify we're in the correct directory
    if not os.path.exists("platformio.ini"):
        print("[ERROR] platformio.ini not found. Please run this script from the project root directory.")
        sys.exit(1)

    # Verify the environment exists
    if not verify_environment(sensor_id):
        sys.exit(1)

    # Clean build artifacts if requested
    if should_clean:
        clean_build_artifacts(sensor_id)

    # Setup SDK config for ESP32-C3
    setup_sdkconfig()

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