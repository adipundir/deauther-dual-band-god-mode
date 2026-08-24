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

## God Mode

God Mode is a one-tap "attack everything in range" mode. From the web UI, tap **God** (or call
`/api/god`); tap **Stop** (or `/api/stop`) to end it. Once on, the device continuously rescans
both bands and deauthenticates every network it finds, hopping channel and band as it goes.

What it does each sweep:

- **Skips protected networks.** Any network you mark **SAFE** (via **Protect** in the UI) is
  left alone, and the device never attacks its own control AP.
- **Focus-fires on networks with clients.** Networks that actually have detected devices get a
  broadcast deauth plus a heavy directed deauth per client, and are revisited on a short cycle.
  Concentrating on client-bearing APs is what keeps devices down; spraying empty networks just
  wastes radio time.
- **Falls back to a broadcast sweep** across all non-SAFE networks when no clients are visible
  yet, so it still disrupts while the next scan discovers clients.
- **Cannot deauth PMF/WPA3 networks.** Those use Protected Management Frames and ignore deauth;
  the UI flags them `[PMF]`.

Because a single radio can only be on one channel at a time, God Mode against many networks is
disruptive rather than a permanent, simultaneous blackout of every one.

## Authorized use only

This is a security-research and education project. Sending deauthentication frames against
networks or devices you do not own or lack explicit written permission to test is illegal in
many jurisdictions. Use it only on your own equipment or within an authorized engagement. The
author accepts no liability for misuse.
