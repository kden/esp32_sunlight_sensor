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
E (310150) WIFI_MANAGER: Wi-Fi disconnected, reason: 8 (wifi_reason_assoc_leave)
I (310160) WIFI_MANAGER: Retrying connection to 'MyWifiSSID' (attempt 1/3)...
I (310200) wifi:flush txq
I (310200) wifi:stop sw txq
I (310200) wifi:lmac stop hw txq
I (310200) SEND_DATA_TASK: Wi-Fi disconnected to save power.
I (314050) LIGHT_SENSOR: ambient light:     163.33 lux
I (314050) SENSOR_TASK: Reading #1 saved (Lux: 163.33)

 ... 

I (602470) LIGHT_SENSOR: ambient light:     163.33 lux
I (602470) SENSOR_TASK: Reading #20 saved (Lux: 163.33)
I (610200) SEND_DATA_TASK: Data send interval reached. Connecting to Wi-Fi...
I (610200) wifi:mode : sta (7c:df:a1:e2:dc:6c)
I (610200) wifi:enable tsf
I (610200) wifi:Set ps type: 0, coexist: 0

I (610200) WIFI_MANAGER: Attempting to connect to network: MyWifiSSID
I (610220) wifi:new:<4,0>, old:<1,0>, ap:<255,255>, sta:<4,0>, prof:1, snd_ch_cfg:0x0
I (610220) wifi:state: init -> auth (0xb0)
I (610230) wifi:state: auth -> assoc (0x0)
I (610250) wifi:state: assoc -> run (0x10)
I (610270) wifi:connected with MyWifiSSID, aid = 14, channel 4, BW20, bssid = b2:c0:3a:b3:a6:35
I (610270) wifi:security: WPA2-PSK, phy: bgn, rssi: -53
I (610270) wifi:pm start, type: 0

I (610280) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (610280) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 25000, mt_pti: 0, mt_time: 10000
I (610340) wifi:dp: 2, bi: 102400, li: 4, scale listen interval from 307200 us to 409600 us
I (610340) wifi:AP's beacon interval = 102400 us, DTIM period = 2
I (610350) wifi:<ba-add>idx:0 (ifx:0, b2:c0:3a:b3:a6:35), tid:0, ssn:0, winSize:64
I (611370) esp_netif_handlers: sta ip: 10.9.132.38, mask: 255.255.255.0, gw: 10.9.132.36
I (611370) WIFI_MANAGER: Successfully connected to 'MyWifiSSID' with IP: 10.9.132.38
I (612200) SEND_DATA_TASK: Sending 20 batched readings.
I (612200) HTTP: Sending JSON with 20 records.
I (613520) HTTP: HTTPS POST request sent successfully, status = 200
I (613530) wifi:state: run -> init (0x0)
I (613540) wifi:pm stop, total sleep time: 0 us / 3260734 us
```

When the ESP32-IDF first boots up and connects to the internet, it will attempt to send two status messages to 
the sensor API, formatted as a list of a single JSON object.  This is so you don't have to wait for five minutes to look at your API logs and see if messages are coming in.

```json
[{"sensor_id": "sensor_1", "timestamp": "2025-07-18T19:26:23Z", "sensor_set_id": "test", "status": "wifi connected"}]
```

```json
[{"sensor_id": "sensor_1", "timestamp": "2025-07-18T19:26:18Z", "sensor_set_id": "test", "status": "ntp set"}]
```

Then it will send 5-minute buffers of messages as JSON lists of objects.

```json
[{"light_intensity": 177.5, "sensor_id": "sensor_1", "timestamp": "2025-07-18T19:16:08Z", "sensor_set_id": "test"}, 
  {"light_intensity": 176.6666717529297, "sensor_id": "sensor_1", "timestamp": "2025-07-18T19:16:23Z", "sensor_set_id": "test"}, 
  {"light_intensity": 175.8333282470703, "sensor_id": "sensor_1", "timestamp": "2025-07-18T19:16:38Z", "sensor_set_id": "test"}, 
  {"light_intensity": 173.3333282470703, "sensor_id": "sensor_1", "timestamp": "2025-07-18T19:16:54Z", "sensor_set_id": "test"}, 
  {"light_intensity": 170.8333282470703, "sensor_id": "sensor_1", "timestamp": "2025-07-18T19:17:09Z", "sensor_set_id": "test"}]
```

## Options

There are a few settings that you can change in the credentials.ini file:

In the `[all_sensors]` section:

- `url`: the URL of the web service where your sensor sends data
- `sensor_set_id`: each location, for example each area of land which will be blanketed with sensors, gets its own sensor_set_id.  I've only tested this with simple IDs with no spaces.

Under each sensor configuration:

- `sensor_id`: a simple identifier for each sensor
- `wifi_credentials`: one or more SSIDs and passwords separated by a colon.  Each pair can be separated by a semicolon, like `SSID:Password;SSID2:Password2`
- `sda_gpio`: Pin number used for SDA GPIO (8 for the ESP32-S3-DevKitC-1U-N8)
- `scl_gpio`: Pin number used for SCL GPIO (9 for the ESP32-S3-DevKitC-1U-N8) 

## Acknowledgments

- BH1750 drivers were provided by Eric Gionet in his project: [ESP32-S3_ESP-IDF_COMPONENTS](https://github.com/K0I05/ESP32-S3_ESP-IDF_COMPONENTS).
  - These drivers are licensed under the MIT License.

## License

This project is licensed under the [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0).
