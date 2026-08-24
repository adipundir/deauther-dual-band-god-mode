# Deauther Dual-Band God Mode - Configuration

This directory contains project configuration files.

## Configuration Structure

- main_config.yaml - Main application configuration 
- logging_config.yaml - Logging configuration
- attack_profiles.yaml - Different attack profiles for various scenarios

## Sample Configuration

```yaml
# Main configuration file
interface: "wlan0"
channel: 6
band: "2.4GHz"  # or "5GHz"
monitor_mode: true
god_mode: false
verbose: false
output_file: "deauth_results.log"
```

## Available Settings

- `interface`: WiFi interface to use (default: wlan0)
- `channel`: Target channel (auto-detect if set to 0)
- `band`: WiFi band to target (2.4GHz or 5GHz)  
- `monitor_mode`: Enable monitor mode
- `god_mode`: Enable God mode with advanced features
- `verbose`: Verbose output
- `output_file`: File to save results