# Deauther Dual-Band God Mode - ESP32-C5 DevKit V2

A WiFi deauthentication tool designed specifically for the ESP32-C5 DevKit V2. This firmware implements advanced dual-band (2.4GHz and 5GHz) deauthentication capabilities with "God mode" functionality to kick off all clients from all networks in range across both frequency bands and channels.

## Features

- Dual-band WiFi deauthentication (2.4GHz and 5GHz)
- God mode for comprehensive network analysis and attack
- Multi-channel scanning and monitoring
- UART communication with host PC
- Real-time packet capture and analysis
- ESP32-C5 hardware-specific optimizations

## Hardware

This project is specifically designed for:
- ESP32-C5 DevKit V2 (WiFi + Bluetooth SoC)
- UART connection to host computer for configuration and status updates
- WiFi capabilities in both 2.4GHz and 5GHz bands
- Full control of embedded WiFi hardware

## Project Structure

- `src/` - Main firmware source code 
- `tests/` - Unit and integration tests (for host-side tools)
- `docs/` - Documentation including hardware specifications, usage guide
- `config/` - Device configuration files
- `scripts/` - Development and deployment utilities
- `firmware/` - ESP-IDF project directory for the embedded firmware

## Usage

For development purposes:

1. Build the firmware: `idf.py build`
2. Flash to device: `idf.py -p /dev/ttyUSB0 flash`
3. Monitor output: `idf.py -p /dev/ttyUSB0 monitor`

To use in God mode:
- Connect via UART to your host PC (default BAUD_RATE = 115200)
- Configure the device in God mode with specific target options
## Control interface

On boot the device hosts a WiFi access point (default SSID `deauther-ctrl`, password
`deauther1234`, set in `firmware/wifi/wifi_controller.h`). Join it from a phone and open
`http://192.168.4.1/` to scan, pick target networks live, and run attacks. **Change this
default AP password before flashing.**

## Authorized use only

This is a security-research and education project. Sending deauthentication frames against
networks or devices you do not own or lack explicit written permission to test is illegal in
many jurisdictions. Use it only on your own equipment or within an authorized engagement. The
author accepts no liability for misuse.
