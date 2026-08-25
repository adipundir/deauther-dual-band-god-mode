# Flashing guide

Two ways to get the firmware onto an ESP32-C5 DevKit: flash the prebuilt image
(no toolchain), or build from source with ESP-IDF.

## 1. Connect the board

- Plug the ESP32-C5 DevKit in over USB (data cable, not charge-only).
- Serial port name:
  - **macOS:** `/dev/cu.usbserial-*` → `ls /dev/cu.usb*`
  - **Linux:** `/dev/ttyUSB0` or `/dev/ttyACM0`; add yourself to `dialout` if you
    get permission errors.
  - **Windows:** `COMx` — check Device Manager → Ports. Install the board's
    USB-UART driver (CP210x / CDC) if no port appears.

## 2a. Flash the prebuilt image (recommended)

The merged image contains bootloader + partition table + app in one file:
[`firmware/prebuilt/deauther_c5_flash_all.bin`](../firmware/prebuilt/), with a
SHA-256 checksum beside it.

```sh
# verify the download (from the folder holding both files)
shasum -a 256 -c deauther_c5_flash_all.bin.sha256     # macOS/Linux
certutil -hashfile deauther_c5_flash_all.bin SHA256   # Windows

pip install esptool        # once

esptool --chip esp32c5 -p <PORT> -b 460800 \
    --before default_reset --after hard_reset \
    write_flash 0x0 deauther_c5_flash_all.bin
```

## 2b. Build from source

Install [ESP-IDF v5.4+](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c5/get-started/)
(v5.5.x is what the release image was built with), then:

```sh
cd firmware
idf.py set-target esp32c5
idf.py build
idf.py -p <PORT> flash monitor
```

`idf.py monitor` attaches to the serial console (115200 baud). Exit with
`Ctrl+]`.

Optional but recommended before flashing a device that will leave your bench:
edit `WIFI_AP_PASS` (and optionally `WIFI_AP_SSID`) in
`firmware/wifi/wifi_controller.h` and rebuild.

## 3. First boot

1. The LED goes dim white (idle) and the log prints
   `Ready — join Wi-Fi 'deauther-ctrl', open http://192.168.4.1/`.
2. Join the `deauther-ctrl` access point (default password `deauther1234`) and open
   `http://192.168.4.1/`.
3. Tap **Scan** and continue with the
   [usage guide](usage_guide.md).

## Troubleshooting

| Symptom | Fix |
|---|---|
| No serial port appears | Try a data USB cable; install the CP210x/CDC driver; check Device Manager / `ls /dev` |
| `A fatal error occurred: Failed to connect` | Hold **BOOT** while pressing **RESET**, then retry |
| Flash succeeds but no LED/log | Re-open the monitor at 115200; press RESET; confirm the bin was written at offset `0x0` |
| Web UI unreachable | Confirm you're associated to `deauther-ctrl`, not another network; the AP re-hosts on channel 1 shortly after `stop`/boot |
| Phone disconnects during scans/attacks | Expected: one radio, so scanning/attacking borrows it. The UI auto-reconnects; Stop restores the AP |
| 5 GHz targets missing from scan | Keep the antenna clear of metal; UNII-3 (149–165) is enabled via country code `"US"`; some DFS channels (52–144) are not scanned |

See also [firmware/README.md](../firmware/README.md) for configuration knobs.
