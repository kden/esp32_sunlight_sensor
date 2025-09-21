# ESP32 Sunlight Sensor

## Table of Contents

- [Description](#description)
- [Introduction](#introduction)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
    - [Hardware](#hardware)
    - [Software](#software)
  - [Installation](#installation)
- [Usage](#usage)
- [Options](#options)
- [Acknowledgements](#acknowledgements)
- [License](#license)

## Description

This project defines the source code for a ESP32 based sunlight sensor. The sensor is designed to measure the intensity of light of and send the results to a web service.

## Introduction

This is my first embedded software project, so there was a lot to learn.  I decided to use an ESP32 board because I knew I'd need WiFi, which is built in.  It also tends to run cheaper than Arduino.

I chose to use ESP-IDF rather than Arduino-style C++ or one of the Python platforms for embedded programming because I wanted to demonstrate that I can still program in C, and I wanted to see if ESP-IDF does a better job at leveraging the ESP32's features than the Arduino libraries.

This is part of the [Sunlight Sensor project in my portfolio](https://kden.github.io/sunlight-sensor/).  You can see more details about the hardware used and other components of the project there.

## Getting Started

### Prerequisites

#### Hardware

This project was built and tested with the following hardware:

- An ESP32 board
  - ESP32 DEVKIT V1 DOIT version with 36 GPIOs [[Amazon Link](https://www.amazon.com/dp/B084KWNMM4)] (V1) 
  - EspressIf ESP32-S3-DevKitC-1U-N8 [[Mouser Link](https://www.mouser.com/ProductDetail/Espressif-Systems/ESP32-S3-DevKitC-1U-N8?qs=Li%252BoUPsLEnt6p%252BOu3d2jKw%3D%3D&countryCode=US&currencyCode=USD)]  This one has the ability to attach an external antenna (indicated by 1U in the model number.)
- BH1750FVI Digital Illumination Sensor Module [[Amazon Link](https://www.amazon.com/gp/product/B09KGXD7C2/)]

#### Software

These are the versions I tested with; other versions may work as well:

- Python 3.10
- esp-idf 6.0 
- PlatformIO 6.1.18

I followed the following guide to set up the esp-idf development environment:

- [ESP-IDF Installation Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html)

Rebelling against popular advice pushing VSCode for embedded development, I followed this guide to use esp-idf with CLion:

- [Working with ESP-IDF in CLion](https://developer.espressif.com/blog/clion/)
  Then I installed PlatformIO locally
  
  ```shell
  pip install platformio
  pio --version
  ```

I ended up using the `pio` command for almost everything instead of the PlatformIO plugin because it was simple and convenient.

### Installation

After you've cloned the repository, copy the `credentials.ini.example` file
to `credentials.ini` and update it to use a valid WiFi network, sensor ID, and bearer token  for your sensor.  Other settings in the `credentials.ini` file are described in the [Options](#options) section. The bearer token is a constant string that will be flashed to the sensor firmware and must be recognized by the API.  One future improvement will be to replace this with a non-constant value.

Create a PEM file for the HTTPs connection to the sensor API and put it in the main directory.

One way to create the PEM file is to run the following command:

```shell
openssl s_client -showcerts -connect  api.example.com:443 </dev/null | openssl x509 -outform PEM > server_cert.pem
```

Copy the appropriate `sdkconfig.defaults_*` file to `sdkconfig.defaults`.  This is how we add options to the `sdkconfig.defaults` file that is picked up by the pio run command.  For the C3 board it has some different options, in particular changing the partition allocation to make room for our verbose logging.

```shell
cp sdkconfig.defaults_esp32c3_base sdkconfig.defaults
```

Once you have PlatformIO installed, you can build the project and upload it to your ESP32
board by running the following commands from the root directory:

```shell
SENSOR_ENV=sensor_1 pio run -e dynamic_sensor -t upload
```

where `sensor_1` is the name of an environment you defined in `credentials.ini`.
`dynamic_sensor` specifies a PlatformIO environment that runs the `dynamic_envs.py` script to 
load specific a specific environment to the hardware you're working with.
The code will run automatically after uploading.

## Usage

To make sure it's working, you'll probably want to monitor the serial output to see the sensor readings:

```shell
pio device monitor
```

You should see the wifi waking up every 5 minutes or so to send a number of readings that were collected every 15 seconds.

```
I (19239) SEND_DATA_TASK: No stored readings to send
I (19249) SEND_DATA_TASK: Successfully processed stored readings
I (25489) SENSOR_TASK: Reading #1 saved (Lux: 4.00)

 ... 

I (310489) SENSOR_TASK: Reading #20 saved (Lux: 8.00)
I (319249) TIME_UTILS: Local time: 2025-09-15 16:48:03 (22:00-04:00) - DAY (active)
I (319249) SEND_DATA_TASK: Data send interval reached. Connecting to WiFi...
I (319249) wifi:Set ps type: 0, coexist: 0

I (319249) SEND_DATA_TASK: Sending 20 batched readings.
I (319259) SEND_DATA_TASK: Filtered 20/20 readings
I (319259) SEND_DATA_TASK: Sensor data send attempt 1/3 (20 filtered readings)
I (319269) HTTP: Sending 20 readings in chunks of 50
I (319269) HTTP: Sending chunk 1-20 of 20 total readings
I (319279) HTTP: Sending JSON chunk with 20 records.
I (320089) HTTP: HTTPS POST request completed, status = 200, content_length = 57
I (320089) HTTP: HTTP request successful with status 200
I (320099) HTTP: Successfully sent chunk. Progress: 20/20 readings
I (320099) HTTP: Successfully sent all 20 readings in 1 chunks
I (320099) SEND_DATA_TASK: Sensor data sent successfully on attempt 1
I (320109) wifi:state: run -> init (0x0)
I (320119) wifi:pm stop, total sleep time: 0 us / 308578140 us
```

When the ESP32-IDF first boots up and connects to the internet, it will attempt to send two status messages to 
the sensor API, formatted as a list of a single JSON object.  This is so you don't have to wait for five minutes to look at your API logs and see if messages are coming in.

```json
[{"sensor_id": "sensor_temp", "timestamp": "2025-09-15T21:06:37Z", "sensor_set_id": "temp", "status": "[boot] wifi connected to Wifi42 IP 10.0.4.71 -47dBm", "commit_sha": "7094935", "commit_timestamp": "2025-09-13 23:07:38 -0500"}]
```

```json
[{"sensor_id": "sensor_temp", "timestamp": "2025-09-15T21:06:43Z", "sensor_set_id": "temp", "status": "[boot] ntp set 2025-09-15 21:06:43 (UTC) / 16:06:43 (local) [valid: yes]", "commit_sha": "7094935", "commit_timestamp": "2025-09-13 23:07:38 -0500"}]
```

Then it will send 5-minute buffers of messages as JSON lists of objects.

```json
[
  {"light_intensity": 32, "sensor_id": "sensor_temp", "timestamp": "2025-09-15T21:57:42Z", "sensor_set_id": "temp"}, 
  {"light_intensity": 32, "sensor_id": "sensor_temp", "timestamp": "2025-09-15T21:57:57Z", "sensor_set_id": "temp"}, 
  {"light_intensity": 31, "sensor_id": "sensor_temp", "timestamp": "2025-09-15T21:58:12Z", "sensor_set_id": "temp"}, 
  {"light_intensity": 14, "sensor_id": "sensor_temp", "timestamp": "2025-09-15T21:58:27Z", "sensor_set_id": "temp"}, 
  {"light_intensity": 15, "sensor_id": "sensor_temp", "timestamp": "2025-09-15T21:58:42Z", "sensor_set_id": "temp"}]
```

At the 5-minute interval, if the board is wired to a battery instead of a USB battery pack, it will also send the battery level as a separate message.

```json
[{"sensor_id": "sensor_temp", "timestamp": "2025-09-15T22:07:40Z", "sensor_set_id": "temp", "status": "[boot] battery", "battery_voltage": 4.01800012588501, "battery_percent": 100, "wifi_dbm": -47, "commit_sha": "7094935", "commit_timestamp": "2025-09-13 23:07:38 -0500"}]
```

## Options

There are a few settings that you can change in the credentials.ini file:

In the `[all_sensors]` section:

- `url`: the URL of the web service where your sensor sends data

Under each sensor configuration:

- `sensor_id`: a simple identifier for each sensor
- `sensor_set_id`: each location, for example each area of land which will be blanketed with sensors, gets its own sensor_set_id.  I've only tested this with simple IDs with no spaces.
- `wifi_credentials`: one or more SSIDs and passwords separated by a colon.  Each pair can be separated by a semicolon, like `SSID:Password;SSID2:Password2`
- `sda_gpio`: pin number used for SDA GPIO (8 for the ESP32-S3-DevKitC-1U-N8)
- `scl_gpio`: pin number used for SCL GPIO (9 for the ESP32-S3-DevKitC-1U-N8) 
- `bearer_token`: simple hard-coded bearer token to authenticate to the sensor API
- `board_type`: a free-format string describing the board
- `night_start_hour`: start of nighttime, a period where we won't take sensor readings or connect to Wifi, to save battery power, in local (sensor-set) time.  Defaults to 22.
- `night_end_hour`: end of nighttime, defaults to 4 (local sensor set time.)
- `battery_adc_gpio`: pin number used to read voltage.  Defaults to -1 which means unused.  Can't be used with the USB battery pack, only with a battery and voltage divider circuit.

## Acknowledgments

- BH1750 drivers were provided by Eric Gionet in his project: [ESP32-S3_ESP-IDF_COMPONENTS](https://github.com/K0I05/ESP32-S3_ESP-IDF_COMPONENTS).
  - These drivers are licensed under the MIT License.

## License

This project is licensed under the [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0).
