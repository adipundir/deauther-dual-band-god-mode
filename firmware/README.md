# ESP32-C5 Dual-Band Deauther

Lean ESP-IDF firmware for the ESP32-C5 DevKit. Scans and deauthenticates on
**both 2.4 GHz and 5 GHz**, driven by a serial CLI.

> Authorized security testing only. Deauthentication disrupts Wi-Fi for every
> client of the targeted AP — only use it on networks you own or are explicitly
> permitted to test.

## Structure

| Path | Role |
|------|------|
| `main/app_main.c` | Entry point + serial CLI + attack task |
| `wifi/` | Dual-band init, scan, channel/band control |
| `deauth/` | 802.11 deauth frame build + raw injection (sanity-check bypass) |
| `godmode/` | Sweep-attack every AP found, both bands |

## How it works

- The radio is held in **STA + promiscuous** so it can hop channels and inject
  raw frames without associating.
- Espressif blocks deauth frames in `esp_wifi_80211_tx()` via
  `ieee80211_raw_frame_sanity_check()`; we override it with
  `-Wl,-wrap=ieee80211_raw_frame_sanity_check` (see `main/CMakeLists.txt`) and a
  `__wrap_` stub in `deauth/deauth_engine.c`.
- 5 GHz uses `esp_wifi_set_band_mode()` (needs ESP-IDF **v5.4+**, which is why
  the C5 is required). 5 GHz injection is newer/less battle-tested than 2.4 GHz
  — verify on your hardware.

## Build & flash

Requires ESP-IDF **v5.4+** (for ESP32-C5 support).

```sh
cd firmware
idf.py set-target esp32c5
idf.py build
idf.py -p /dev/cu.usbserial-110 flash monitor   # macOS UART port
```

## CLI (UART0 @ 115200)

```
scan            scan 2.4 + 5 GHz
list            show scan results (with index, band, channel)
deauth <index>  continuously deauth one AP from the list
god             GOD MODE — deauth every AP found, both bands, on a loop
stop            stop attacking
help            list commands
```
