while true; do
    python -m serial.tools.miniterm /dev/ttyACM0 115200 --raw
    sleep 1
done