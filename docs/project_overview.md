# Deauther Dual-Band God Mode - ESP32-C5 DevKit V2

A WiFi deauthentication tool designed specifically for the ESP32-C5 DevKit V2 that includes both embedded firmware and a mobile-accessible web UI.

## Features

- Dual-band WiFi deauthentication (2.4GHz and 5GHz)
- God mode for comprehensive network analysis and attack
- Multi-channel scanning and monitoring  
- UART communication with host PC for configuration
- Mobile-accessible web UI through HTTP server
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
- `ui/` - Web UI files (mobile-accessible)

## Usage

For development purposes:

1. Build the firmware: `idf.py build`  
2. Flash to device: `idf.py -p /dev/ttyUSB0 flash`
3. Monitor output: `idf.py -p /dev/ttyUSB0 monitor`
4. Access web UI from any mobile device connected to the same network

To use in God mode:
- Connect via UART to your host PC (default BAUD_RATE = 115200)
- Configure the device in God mode with specific target options
- Access web interface on port 80 of the device's IP address