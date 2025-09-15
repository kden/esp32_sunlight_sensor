"""
get_git_info.py

A pre-build script for PlatformIO to fetch the current Git commit SHA and
timestamp and write them to a C header file for firmware inclusion.

Copyright (c) 2025 Caden Howell (cadenhowell@gmail.com)
Developed with assistance from Gemini Code Assist (2025).
Apache 2.0 Licensed as described in the file LICENSE
"""
import subprocess
import os
from datetime import datetime

def get_git_info():
    try:
        # Get the full commit SHA and truncate to 7 characters
        sha = subprocess.check_output(['git', 'rev-parse', 'HEAD']).decode('utf-8').strip()[:7]

        # Get the commit timestamp as Unix timestamp, then convert to UTC
        timestamp_unix = subprocess.check_output([
            'git', 'log', '-1', '--format=%ct'
        ]).decode('utf-8').strip()

        # Convert Unix timestamp to UTC ISO format matching other timestamps
        dt = datetime.utcfromtimestamp(int(timestamp_unix))
        timestamp = dt.strftime('%Y-%m-%dT%H:%M:%SZ')

        return sha, timestamp
    except subprocess.CalledProcessError:
        return "unknown", "unknown"
    except FileNotFoundError:
        return "no-git", "no-git"

def generate_header():
    sha, timestamp = get_git_info()

    header_content = f'''/**
 * Auto-generated Git version information
 * DO NOT EDIT MANUALLY
 */
#pragma once

#define GIT_COMMIT_SHA "{sha}"
#define GIT_COMMIT_TIMESTAMP "{timestamp}"
'''

    # Ensure the include directory exists
    os.makedirs('include', exist_ok=True)

    with open('include/git_version.h', 'w') as f:
        f.write(header_content)

    print(f"Generated git version info: {sha} at {timestamp}")

if __name__ == "__main__":
    generate_header()