# ESP32-C5 Integration Guide

This document outlines how our deauther system integrates with the ESP32-C5 DevKit V2 hardware.

## Hardware Specifications

- **Chip**: ESP32-C5 (WiFi + Bluetooth SoC)
- **Memory**: 4MB Flash, 160KB SRAM
- **Interfaces**: 
  - UART for host communication
  - WiFi (2.4GHz and 5GHz)
  - Bluetooth LE (Bluetooth 5.4)

## UART Communication

The device uses UART for communication with the host PC:

- **Baud Rate**: 115200
- **Port**: `/dev/ttyUSB0` (default)
- **Data Format**: 8N1 (8 data bits, no parity, 1 stop bit)

### Commands

Available commands through UART:
- `help` - Show available commands  
- `start` - Start deauthentication attack
- `stop` - Stop current attack
- `scan` - Scan for networks on both bands
- `godmode` - Enable God mode (target all networks)
- `status` - Show current device status

### Response Format

Responses are sent in simple text format:
```
[STATUS] [DESCRIPTION]
```

Examples:
```
[INFO] Scanning for networks...
[SUCCESS] Scan complete. Found 3 networks.
[ERROR] Invalid channel number
```

## WiFi Capabilities

The ESP32-C5 supports:

1. **Dual-Band Operation**: Simultaneous operation on 2.4GHz and 5GHz
2. **Channel Scanning**: Auto-detection of available channels
3. **Deauthentication Frames**: Generation and transmission of deauth frames
4. **Bandwidth Options**: Support for 20MHz, 40MHz, 80MHz channels 

## Web UI Integration

The embedded HTTP server on port 80 provides:

- Mobile-accessible web interface  
- Real-time status updates
- Configuration controls
- Network scanning results
- Log output and troubleshooting information

### Mobile Access

To access the UI from any mobile device:
1. Connect your mobile device to the ESP32's network (default: deauther-network)
2. Open browser to http://192.168.4.1/
3. Use the controls to configure and start attacks

## Firmware Structure

The firmware is organized as:

- `main/` - Main entry point
- `wifi/` - WiFi control implementation 
- `uart/` - UART communication handlers  
- `http_server/` - Embedded HTTP server for UI
- `deauth/` - Deauthentication attack engine
- `godmode/` - God mode functionality

This structure allows for:
- Easy testing and debugging
- Modular development
- Scalable expansion 
- Clear separation of concerns