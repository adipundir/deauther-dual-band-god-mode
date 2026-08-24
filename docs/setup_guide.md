# ESP32-C5 Project Setup & Installation Guide

This document provides comprehensive instructions for setting up your complete Deauther God Mode project environment on macOS.

## Prerequisites

Before you begin, ensure you have:

### Hardware:
- ESP32-C5 DevKit V2 board
- Micro USB cable for programming and serial communication  
- Computer with USB ports available (macOS recommended)

### Software Requirements:
- macOS 10.15+ or Linux (for development)
- Python 3.8+
- Git version control system  
- Terminal/Console application

## Initial Project Setup

### 1. Clone the Repository
```bash
# Navigate to your projects directory
cd ~/Documents/Projects/

# Create project directory and clone 
mkdir deauther-dual-band-god-mode
cd deauther-dual-band-god-mode

# If this is an existing project, clone with:
git clone <your-repo-url> .
```

### 2. Project Structure Overview
The project follows this structure:
```
deauther-dual-band-god-mode/
├── README.md
├── src/                    # Source code files 
├── tests/                    # Test files
├── docs/                   # Documentation
├── config/                 # Configuration files
├── firmware/             # ESP-IDF project for embedded firmware  
├── ui/                   # Web user interface (HTML/CSS/JS)
├── requirements.txt        # Python dependencies
└── scripts/                  # Utility and deployment scripts
```

## Development Environment Setup

### 1. Install ESP-IDF Framework (ESP32-C5 Support)
```bash
# Create ESP directory if not exists  
cd ~
mkdir esp
cd esp

# Clone the ESP-IDF repository
git clone https://github.com/espressif/esp-idf.git
cd esp-idf

# Install dependencies 
./install.sh

# Source the environment (add to your ~/.zshrc or ~/.bash_profile)
source export.sh

# Verify installation
idf.py --version
```

### 2. Configure PATH and Environment Variables
Add these to your shell configuration file (`~/.zshrc` for macOS, `~/.bashrc` for Linux):

```bash
# ESP-IDF setup
export IDF_PATH=~/esp/esp-idf
source ~/esp/esp-idf/export.sh
```

After adding these lines:
```bash
source ~/.zshrc  # or source ~/.bashrc for bash
```

### 3. Install Python Dependencies  
```bash
# Navigate to project directory
cd ~/deauther-dual-band-god-mode

# Install requirements
pip install -r requirements.txt
```

## USB-to-Serial Driver Setup

### macOS:
The ESP32-C5 typically uses a CP210x USB-to-serial converter. Check if drivers are installed:

```bash
ls /dev/tty*
# Should show something like: /dev/ttyUSB0 or /dev/cu.SLAB_USBtoUART  
```

If not available, install the Silicon Labs driver:
1. Download from: https://www.silabs.com/developers/usb-to-serial-converters
2. Install CP210x USB to UART Bridge VCP Drivers 
3. Reconnect your ESP32-C5

### Linux:
```bash
# Check if device is detected  
ls /dev/ttyUSB*

# Install required packages (Ubuntu/Debian)
sudo apt update
sudo apt install libusb-dev

# Add user to dialout group for serial access
sudo usermod -a -G dialout $USER
```

### Windows:
1. Download Silicon Labs CP210x USB to UART Bridge VCP Drivers  
2. Install the driver
3. Check in Device Manager under "Ports (COM & LPT)"  

## Development Workflow

### 1. Development on Host Computer
The host computer runs:
- Python scripts for documentation generation and testing
- Utility tools
- Communication with embedded device over UART

### 2. ESP32-C5 Firmware Development
The ESP-IDF project in `firmware/` contains:
- WiFi control implementation 
- UART communication handlers  
- HTTP server for web UI access
- Deauthentication attack engine  
- God mode functionality

### 3. Web Interface Development
Web files in `ui/` contain:
- Responsive HTML UI with CSS styling
- JavaScript for interactive controls
- Mobile-compatible design

### 4. Build Process 
```bash
# Navigate to firmware directory 
cd firmware/

# Build the project
idf.py build

# Flash to device (replace with your actual port)
idf.py -p /dev/ttyUSB0 flash

# Monitor output
idf.py -p /dev/ttyUSB0 monitor
```

## Testing and Verification 

### 1. Test Installation
```bash
# Verify all components are working correctly 
python src/main.py --help

# Should show usage information 
```

### 2. Test Web Interface Access
```bash
# Run with UI server
python src/main.py --ui --verbose

# Browser should be able to access web interface  
# from any mobile device connected to the same network
```

## Common Issues and Solutions

### 1. ESP-IDF Not Found
```bash 
# Error:
# Command not found: idf.py

# Solution: Add environment source to shell profile
echo 'source ~/esp/esp-idf/export.sh' >> ~/.zshrc
source ~/.zshrc
```

### 2. Permission Denied on Serial Port
```bash
# Error:
# Permission denied on /dev/ttyUSB0

# Solution: 
sudo chmod 666 /dev/ttyUSB0
# or add user to dialout group on Linux
```

### 3. Device Not Found After Connection
Ensure USB cable and drivers are working:
```bash
ls /dev/tty*
# Look for newly connected device after plugging in ESP32-C5  
```

## Next Steps - Flashing Your Device

Once development environment is complete:

1. **Configure firmware settings** in `firmware/` as needed
2. **Build the firmware**: `idf.py build`
3. **Flash to ESP32-C5**: `idf.py -p /dev/ttyUSB0 flash`  
4. **Verify connection and access web UI**

For detailed flashing instructions, see `docs/flashing_guide.md`.

This setup ensures your complete Deauther God Mode system is ready for development of the embedded firmware components that will run on your ESP32-C5 hardware.