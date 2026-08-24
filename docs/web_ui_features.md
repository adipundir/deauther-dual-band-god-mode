# Deauther God Mode Web UI - Features

This document describes the complete web user interface for your ESP32-C5 dual-band God mode deauther system.

## Overview

The web UI provides real-time monitoring and control of your deauthentication operations. It's accessible from any mobile device or computer connected to the same network as your ESP32-C5 through a browser at `http://<device-ip>` address.

## Main Interface Features

### 1. Device Status Panel
- Current system status (Running/Stopped/Scanning)
- Device IP address
- Connected networks count
- Active attack mode

### 2. Network Scanning Results  
- All detected networks in range
- Network details:
  - SSID
  - BSSID (MAC address)
  - Channel 
  - Band (2.4GHz or 5GHz)
  - Signal strength (RSSI)
  - Number of connected devices

### 3. Device Details  
For each network, show all connected devices:
- Device MAC address
- Device name (if available)
- Connection type
- Signal strength
- Connection time

### 4. Control Panel
- **Start/Stop Deauth**: Initiate or stop deauthentication attacks  
- **Network Scan**: Perform new network scanning
- **God Mode**: Enable God mode to target all networks simultaneously
- **Configuration Options**:
  - Band selection (2.4GHz, 5GHz, or both) 
  - Channel specification
  - Attack type selection

### 5. Deauth Actions
- **Deauth Single Device**: Select and deauthenticate a specific device
- **Deauth Network**: Deauthenticate all devices from a selected network  
- **Deauth All**: Deauthenticate ALL devices from ALL networks (God mode)

## God Mode Functionality

When enabled:
- Scans and targets ALL networks in range across both 2.4GHz and 5GHz bands
- Automatically monitors all available channels 
- Launches coordinated deauthentication attacks across the entire WiFi spectrum
- Displays comprehensive statistics of attack scope

## Mobile Responsiveness

The UI is fully responsive and optimized for mobile use:
- Touch-friendly controls
- Adaptive layout for small screens
- Real-time status updates without page refresh
- Quick access to critical functions

## Technical Implementation

### Network Display Format:
```
Network: HomeWiFi (AA:BB:CC:DD:EE:FF)
  Channel: 6, Band: 2.4GHz, RSSI: -52dBm
  Connected Devices: 3

  Device 1: 11:22:33:44:55:66 (Client1) - RSSI: -60dBm
  Device 2: 77:88:99:AA:BB:CC (Client2) - RSSI: -65dBm  
  Device 3: DD:EE:FF:11:22:33 (Client3) - RSSI: -62dBm
```

### Control Flow:
1. **Scan Networks** (initiates WiFi scanning)
2. **View Results** (displays networks and connected devices)  
3. **Select Action** (choose single device, entire network, or God mode)
4. **Execute** (launch deauthentication attack)

## Expected UI Components

The web interface should contain:
1. Device Status Block
2. Network Summary Table  
3. Device Details Sections
4. Control Buttons
5. Configuration Options
6. Log Output Panel
7. Responsive Layout for Mobile Devices

This interface will make your ESP32-C5 deauther system completely accessible and controllable through any web browser on any device, exactly as requested.