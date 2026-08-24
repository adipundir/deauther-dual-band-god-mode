# ESP32-C5 Dual-Band Deauther — firmware

ESP-IDF (v5.5.x) firmware for the ESP32-C5 DevKit. Scans both bands, sniffs
associated clients, and injects 802.11 deauth/disassoc frames on **2.4 GHz and
5 GHz**. Controlled from the on-device web UI (`http://192.168.4.1/`) or the serial
console on UART0 @ 115200.

> Authorized security testing only. Deauthentication disrupts Wi-Fi for every client
> of the targeted AP — only use it on networks you own or are explicitly permitted
> to test.

## Components

| Path | Role |
|------|------|
| `main/app_main.c` | Entry point; registers the serial console commands |
| `wifi/` | Dual-band init, scan, channel/band control, promiscuous client sniffing |
| `deauth/` | Deauth/disassoc frame builder + raw TX engine, TX stats |
| `control/` | Attack-mode state machine + attack/stats FreeRTOS tasks (shared surface for UI and CLI) |
| `godmode/` | God Mode sweep: every non-SAFE network, focus-fired on client-bearing APs |
| `http_server/` | Embedded HTTP server hosting the single-page control UI |
| `led/` | WS2812 status LED (idle/scanning/counting/attacking/god) |

## How injection works

Espressif's driver refuses to TX deauth/disassoc frames: `esp_wifi_80211_tx()` runs
them through `ieee80211_raw_frame_sanity_check()`. This project overrides that check
at link time — `-Wl,-wrap=ieee80211_raw_frame_sanity_check` is set as a global link
option in `main/CMakeLists.txt`, and `__wrap_ieee80211_raw_frame_sanity_check()` in
`deauth/deauth_engine.c` always returns 0 (ESP_OK). No IDF sources or binaries are
modified.

Frames sent per target cycle:

- deauth (subtype `0xC0`, reason 7) and disassoc (`0xA0`, reason 8), broadcast and
  directed to each discovered client;
- directed frames go out both directions (AP→client spoofed from the AP, and
  client→AP spoofed from the client) so both sides tear down;
- 2.4 GHz injects through the AP interface; 5 GHz parks the AP on the target channel
  first (`wifi_park_channel`) because a 2.4 GHz-only AP cannot hold the radio on a
  5 GHz channel.

## Build & flash

Requires ESP-IDF v5.4+ (v5.5.x tested) with the esp32c5 target installed:

```sh
idf.py set-target esp32c5
idf.py build
idf.py -p /dev/cu.usbserial-110 flash monitor   # adjust port
```

A prebuilt merged image lives in [`prebuilt/`](prebuilt/) — flash it without a
toolchain:

```sh
esptool --chip esp32c5 -p <PORT> -b 460800 \
    --before default_reset --after hard_reset write_flash 0x0 prebuilt/deauther_c5_flash_all.bin
```

## Serial CLI

```
scan                                   # dual-band scan + client sniff
list                                   # networks as JSON
dev                                    # count stations per AP
deauth <i> [i...]                      # attack selected APs (directed + broadcast)
strike <ch> <is5> <bssid> <mac>        # parked directed kill of one known client
hunt <mac> <ch> <is5> <bssid> [...]    # chase one client across up to 4 bands
god                                    # GOD MODE — every non-SAFE network
protect <i> [i...]                     # toggle SAFE (God Mode skips these)
stop                                   # stop attacking, restore AP + STA
heap                                   # free/min heap
txtest                                 # harmless TX self-test (fake BSSID)
```

## Configuration worth knowing

- Control AP SSID/password/channel: `wifi/wifi_controller.h`
  (`WIFI_AP_SSID` / `WIFI_AP_PASS` / `WIFI_AP_HOME_CH`). Change the password before
  flashing if the device will run anywhere others can reach it.
- Country code is set to `"US"` in `wifi/wifi_controller.c` to unlock 5 GHz UNII-3
  (ch 149–165) scanning/injection regardless of the flashed default.
- `sdkconfig.defaults`: 1000 Hz FreeRTOS tick, dynamic TX buffers (64) so high-rate
  raw injection doesn't stall.
- Status LED GPIO: `led/led.c` (`LED_GPIO`, default 27).

## Partitions

`partitions.csv` keeps the stock single-factory-app layout with a large app slot
(0x2C0000); the current build uses ~63% of it.
