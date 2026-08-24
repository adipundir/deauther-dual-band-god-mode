# ESP32-C5 Flashing Guide

This document provides detailed instructions for flashing your Deauther God Mode firmware to the ESP32-C5 DevKit V2 hardware.

## Hardware Requirements

### ESP32-C5 DevKit V2
- ESP32-C5 chip with dual-band WiFi capability  
- Built-in USB-to-serial converter
- Development board with standard USB connection
- GPIO pins for expansion (if needed)

### Connection Requirements  
- Micro USB cable for programming and serial communication
- Computer or laptop with USB ports available

## Required Software & Drivers

### 1. ESP-IDF Framework (ESP32-C5)
The firmware is built using the ESP-IDF (Espressif IoT Development Framework):

Install ESP-IDF using the official documentation:
```bash
# For macOS/Linux users
cd ~
mkdir esp
cd esp
git clone https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh
```

### 2. USB-to-Serial Drivers

For ESP32-C5 on macOS, the drivers are typically included with:
- Arduino IDE (includes CP210x driver)
- PlatformIO
- Or install directly from Silicon Labs: https://www.silabs.com/developers/usb-to-serial-converters

On Windows:
- Install Silicon Labs CP210x USB to UART Bridge VCP Drivers  
- Available at: https://www.silabs.com/developers/usb-to-serial-converters

### 3. Python and Dependencies
Make sure you have:
```bash
# Python 3.8+ is recommended
python3 --version

# Required pip packages for ESP-IDF
pip install -r requirements.txt
```

## Prerequisites Setup  

### Step 1: Install ESP-IDF
```bash
# In your project directory 
cd ~/deauther-dual-band-god-mode

# Follow ESP-IDF installation from Espressif's documentation:
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html
```

### Step 2: Set Up Environment
```bash
# Source the ESP-IDF environment
source ~/esp/esp-idf/export.sh

# Verify the setup
idf.py --version
```

### Step 3: Verify USB Connection
```bash
# On macOS/Linux systems:
ls /dev/ttyUSB* 
ls /dev/cu*

# Should show something like:
# /dev/ttyUSB0   or  
# /dev/cu.SLAB_USBtoUART
```

## Flashing Process

### 1. Prepare the Firmware Source Code
```bash
# Ensure you are in the project directory
cd ~/deauther-dual-band-god-mode

# Navigate to firmware directory
cd firmware/
```

### 2. Configure Build Settings (if needed)  
The project should include an `sdkconfig` file with appropriate settings:
```bash
# Check your SDK config if it exists
ls -la sdkconfig

# If not, you can configure default settings
idf.py menuconfig
```

### 3. Build the Firmware
```bash
# Clean any previous builds
idf.py clean

# Build the project
idf.py build
```

### 4. Flash to ESP32-C5 DevKit
Before flashing, ensure your device is connected and detected:
```bash
# Verify device is connected
ls /dev/tty*

# If needed, set the correct port (adjust the port name as needed)
export ESPPORT=/dev/ttyUSB0
# or on macOS:
# export ESPPORT=/dev/cu.SLAB_USBtoUART
```

Then flash:
```bash
# Flash using idf.py tool
idf.py -p /dev/ttyUSB0 flash

# Or if using a different port
idf.py -p /dev/cu.SLAB_USBtoUART flash
```

### 5. Monitor the Output
To see real-time output from your device:
```bash
# Monitor the serial output
idf.py -p /dev/ttyUSB0 monitor

# This should show logs from your firmware (similar to):
# [09:15:32] System initialized  
# [09:15:33] WiFi interfaces configured
# [09:15:35] Starting network scan...
```

## Troubleshooting Common Issues

### 1. Device Not Detected
```bash
# Check USB connection:
ls /dev/tty*

# If no device found:
# - Try different USB cable 
# - Try different USB port on computer
# - Ensure USB-to-serial drivers are installed correctly
```

### 2. Permission Denied Errors  
On macOS/Linux, you might get permission issues:
```bash
# Add current user to dialout group (Linux)
sudo usermod -a -G dialout $USER

# Or run with sudo permissions
sudo idf.py -p /dev/ttyUSB0 flash

# Or configure udev rules on Linux for better permissions
```

### 3. Flashing Errors  
```bash
# Clean build and retry
idf.py clean
idf.py build
idf.py -p /dev/ttyUSB0 flash

# If still failing, verify board version  
# ESP32-C5 might need specific partition tables
```

### 4. No Serial Output After Flashing 
After flashing:
- Wait ~10 seconds after flashing completes
- Check USB connection is secure
- Try with different USB cable or port
- Ensure there's no other process using the serial port

## Post-Flashing Verification

Once flashed successfully, your device should:

### 1. Boot Sequence
```
[00:00:00] ESP-IDF boot sequence started
[00:00:01] Initializing system components...  
[00:00:02] WiFi driver initialized
[00:00:03] HTTP server starting on port 80
[00:00:04] UART interface ready at 115200 baud
[00:00:05] System ready. Connect to deauther-network
```

### 2. Network Access  
Connect your mobile device to the WiFi network broadcast by the ESP32-C5:
- Network name: typically `deauther-network` 
- IP address: usually `192.168.4.1`
- Access web UI from mobile browser

### 3. Serial Output Verification
```bash
# Start serial monitor to check operations:
idf.py -p /dev/ttyUSB0 monitor

# You should see device status information and system logs
```

## Required Tools for Development

The following tools are recommended:

1. **ESP-IDF** (Espressif IoT Development Framework)
2. **Python 3.8+**
3. **Git** (for version control) 
4. **Terminal/Console application**
5. **Text editor or IDE** (VS Code, CLion, etc.)
6. **Serial Terminal** (screen, minicom, or PlatformIO serial monitor)

## Configuration Notes

### For ESP32-C5 Specific Considerations:
- Dual-band operation requires specific memory allocation
- UART configuration: 115200 baud rate, 8N1 format
- HTTP server running on port 80
- WiFi channel scanning capabilities for both bands

## Additional Resources  

### Official ESP-IDF Documentation:
- https://docs.espressif.com/projects/esp-idf/en/latest/
- https://docs.espressif.com/projects/esp32-c5/en/latest/

### Support Forums:
- https://esp32.com/
- https://github.com/espressif/esp-idf
- ESP-IDF GitHub repository documentation

This complete guide provides all necessary steps to flash your Deauther God Mode firmware to the ESP32-C5 DevKit V2 for full dual-band operation and mobile UI functionality.