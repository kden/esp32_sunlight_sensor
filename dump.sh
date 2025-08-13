#!/bin/sh

# This script prints the contents of all .h files in the include/ directory
# and all .c files in the main/ directory, with a formatted header
# showing the filename above the content of each file.

for file in include/*.h main/*.c platformio.ini sdkconfig.defaults credentials.ini.example dynamic_envs.py; do
  if [ -f "$file" ]; then
    echo "----------"
    echo "$file"
    echo "----------"
    cat "$file"
    echo "\n"
  fi
done