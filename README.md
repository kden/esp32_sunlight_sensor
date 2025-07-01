# ESP32 Sunlight Sensor

## Table of Contents
- [Description](#description)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
    - [Hardware](#hardware)
    - [Software](#software)
  - [Installation](#installation)
- [Usage](#usage)
- [License](#license)


## Description
This project defines the source code for a ESP32 based sunlight sensor. The sensor is designed to measure the amount 
of sunlight and display the results on an OLED display.

## Getting Started

### Prerequisites
#### Hardware
This project was built and tested with the following hardware:
- ESP32 DEVKIT V1 DOIT version with 36 GPIOs [[Amazon Link](https://www.amazon.com/dp/B084KWNMM4)]
- BH1750FVI Digital Illumination Sensor Module [[Amazon Link](https://www.amazon.com/gp/product/B09KGXD7C2/)]
- SSD1306 0.96" 128x64 OLED Display Module [[Amazon Link](https://www.amazon.com/gp/product/B06XRBYJR8)]

#### Software
These are the versions I tested with; other versions may work as well:
- Python 3.10
- esp-idf 6.0 
- PlatformIO 6.1.18

I followed the following guide to set up the esp-idf development environment:
- [ESP-IDF Installation Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html)

 
Contrary to popular advice pushing VSCode for all embedded development, I followed this guide to use esp-idf with CLion:
- [Working with ESP-IDF in CLion](https://developer.espressif.com/blog/clion/)
Then I installed PlatformIO locally
```shell
pip install platformio
pio --version
```
I ended up using the `pio` command for almost everything 
instead of the PlatformIO plugin because it was simple and convenient.

### Installation
After you've cloned the repository, copy the `credentials.ini.example` file
to `credentials.ini` and update it to 
use a valid WiFi network, sensor ID, and bearer token  for your sensor.  For now, the bearer token 
is a constant string that will be flashed to the sensor firmware and must be recognized by the API.

Create a PEM file for the HTTPs connection to the sensor API and put it in the main directory.

One way to create the PEM file is to run the following command:

```shell
openssl s_client -showcerts -connect  api.example.com:443 </dev/null | openssl x509 -outform PEM > server_cert.pem
````

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

## Acknowledgments
- BH1750 and SSD1306 drivers were provided by Eric Gionet in his project: [ESP32-S3_ESP-IDF_COMPONENTS](https://github.com/K0I05/ESP32-S3_ESP-IDF_COMPONENTS).
  - These drivers are licensed under the MIT License.

## License
This project is licensed under the [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0).


