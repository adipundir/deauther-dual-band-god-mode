# Deauther God Mode Project Structure

This document describes the full structure and components of our dual-band God mode deauther project for ESP32-C5.

## High-Level Architecture

1. **Embedded Firmware** (in `firmware/`):
   - ESP-IDF based implementation
   - WiFi control capabilities for both 2.4GHz and 5GHz
   - UART interface for host communication  
   - HTTP server for web UI

2. **Web Interface** (in `ui/`):
   - Mobile-responsive HTML/CSS/JS frontend
   - Real-time status updates
   - Control panel for deauth operations
   - Network scanning and display

3. **Host Communication**:
   - UART-based configuration interface  
   - HTTP server on embedded device
   - Status reporting to web UI

## Key Features Implementation

### 1. Dual-Band Support
- Automatic detection of 2.4GHz and 5GHz networks
- Simultaneous monitoring across both bands
- Channel switching capabilities

### 2. God Mode Functionality  
- Comprehensive network scanning
- Targets all clients on all networks
- Multi-channel attack sequences
- System-wide deauthentication capabilities

### 3. Web UI Components
- Device status panel
- Configuration controls  
- Network scan results
- Real-time logging output
- Mobile-responsive design

## Development Roadmap

1. **Firmware Core**: Basic WiFi control and UART interface
2. **Deauth Engine**: Dual-band scanning and deauth logic
3. **HTTP Server**: Embedded web server for UI access  
4. **God Mode Implementation**: Full network targeting capabilities
5. **Mobile UI**: Responsive web interface with real-time updates

## Integration Points

- Serial communication via UART (`/dev/ttyUSB0`)
- HTTP server on port 80 when connected to network
- WiFi scanning and deauth operations
- Status reporting to both serial and HTTP interfaces