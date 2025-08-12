#!/usr/bin/env python3
"""
Script to sync real source files to test directory for PlatformIO native testing
This ensures we're testing the actual production code
"""

import os
import shutil
from pathlib import Path

def sync_file(src_path, dest_path):
    """Copy source file to destination, creating directories if needed"""
    dest_path.parent.mkdir(parents=True, exist_ok=True)

    if src_path.exists():
        shutil.copy2(src_path, dest_path)
        print(f"Synced: {src_path} -> {dest_path}")
        return True
    else:
        print(f"Warning: Source file not found: {src_path}")
        return False

def main():
    # Define the files to sync
    files_to_sync = [
        ("main/task_send_data.c", "test/test_task_send_data_integrated/task_send_data.c"),
        ("test/mocks/esp_mocks.c", "test/test_task_send_data_integrated/esp_mocks.c"),
    ]

    project_root = Path(__file__).parent
    synced_count = 0

    print("Syncing test files...")

    for src_rel, dest_rel in files_to_sync:
        src_path = project_root / src_rel
        dest_path = project_root / dest_rel

        if sync_file(src_path, dest_path):
            synced_count += 1

    print(f"Synced {synced_count}/{len(files_to_sync)} files")

    if synced_count == len(files_to_sync):
        print("✅ All test files synced successfully!")
        return 0
    else:
        print("❌ Some files failed to sync")
        return 1

if __name__ == "__main__":
    exit(main())