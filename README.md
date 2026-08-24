# Deauther Dual-Band God Mode — ESP32-C5

An authorized-use Wi-Fi security testing tool for the **ESP32-C5 DevKit**. It scans
both Wi-Fi bands (2.4 GHz and 5 GHz), discovers which devices are connected to each
access point, and sends IEEE 802.11 deauthentication / disassociation frames to test
how networks and clients respond to management-frame attacks — including a one-tap
**God Mode** that sweeps every network in range.

> **Authorized use only.** Sending deauthentication frames to networks or devices you
> do not own, or lack explicit written permission to test, is illegal in most
> jurisdictions. Use it exclusively on your own equipment or within an authorized
> engagement.

---

## What it does

| Capability | Details |
|---|---|
| **Dual-band scan** | Active scan across 2.4 GHz (ch 1–13) and 5 GHz (incl. UNII-3, ch 149–165). Lists SSID, BSSID, band, channel, signal, security, and detected PMF/WPA3. |
| **Client discovery** | Promiscuous channel-hopping sniffing records the station MACs behind each AP (RSSI + frame count per device). |
| **Targeted attack** | Pick APs in the web UI → continuous broadcast + directed (unicast) deauth/disassoc to every discovered client, non-stop until stopped. |
| **God Mode** | Attacks every network in range except ones you marked SAFE and its own control AP. Focus-fires on client-bearing networks and re-scans every 45 s to pick up newcomers. |
| **SAFE protection** | Toggle any network as SAFE (web UI *Protect* button or serial `protect` command). SAFE networks are skipped by God Mode; protection survives re-scans because it is keyed by BSSID. |
| **Strike** | Serial-only directed kill of one known client on one known BSSID/channel — no discovery needed. |
| **Hunt** | Serial-only chase of one client across up to four bands/BSSIDs, alternating fast enough that a band-steering roamer has nowhere to settle. |
| **Web UI** | Served by the device itself at `http://192.168.4.1/` — scan, live packet counters, per-AP device lists, attack/god/stop controls. |
| **Serial CLI** | Full console on UART0 @ 115200 for everything above plus diagnostics (`heap`, `txtest`). |
| **Status LED** | Onboard WS2812: white idle · blue scanning · cyan counting · red blinking attacking · rainbow God Mode. |

## What it cannot do (physics, not bugs)

- **One radio = one channel at a time.** Attacking many channels is a disruptive hop,
  not a simultaneous blackout of all of them.
- **PMF/WPA3 networks are immune.** Protected Management Frames make deauth frames
  unspoofable. The UI flags these `[PMF]`.
- **Band-steering routers defeat a single radio.** A client that roams seamlessly to
  the other band of the same SSID can dodge an alternating attack (`hunt` narrows but
  does not close this gap; two boards parked per band do).
- **5 GHz injection is weaker than 2.4 GHz** on this silicon (lower effective rate,
  fewer frames/sec).

## Hardware

- ESP32-C5 DevKit (DevKit V2 tested) — dual-band 802.11a/b/g/n/ac radio.
- USB cable for flashing + serial console.
- Onboard WS2812 RGB LED on GPIO 27 (change `LED_GPIO` in `firmware/led/led.c` for
  other boards).

## Quick start — flash the ready-made image

No toolchain needed. Download `firmware/prebuilt/deauther_c5_flash_all.bin`
(merged bootloader + partition table + app, verified SHA-256 alongside it) and flash
it at offset `0x0`:

```sh
pip install esptool                      # once
esptool --chip esp32c5 -p <PORT> -b 460800 \
    --before default_reset --after hard_reset \
    write_flash 0x0 deauther_c5_flash_all.bin
```

Serial port by OS: macOS `/dev/cu.usbserial-*` · Linux `/dev/ttyUSB0` or
`/dev/ttyACM0` · Windows `COMx` (see Device Manager).

Verify the checksum before flashing:

```sh
shasum -a 256 -c deauther_c5_flash_all.bin.sha256     # macOS/Linux
```

## Building from source

Requires [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c5/get-started/)
**v5.4+** (v5.5.x used for the prebuilt image; the C5 needs 5.4+ for
`esp_wifi_set_band_mode()`).

```sh
cd firmware
idf.py set-target esp32c5
idf.py build
idf.py -p <PORT> flash monitor
```

Notes:

- Deauth TX is enabled by a linker-level override:
  `-Wl,-wrap=ieee80211_raw_frame_sanity_check` (see `firmware/main/CMakeLists.txt`)
  with the `__wrap_` stub in `firmware/deauth/deauth_engine.c`. No binary patching of
  the IDF Wi-Fi library is involved.
- Change the control-AP credentials before flashing: `WIFI_AP_SSID` /
  `WIFI_AP_PASS` in `firmware/wifi/wifi_controller.h` (defaults: `deauther-ctrl` /
  `deauther1234`). The defaults are fine for bench testing; change them if you keep
  the device running anywhere others can reach it.

## Using it

1. Flash and power the board. It hosts a WPA2 access point and boots idle.
2. Join `deauther-ctrl` (default password `deauther1234`) from your phone/laptop.
3. Open `http://192.168.4.1/`.
4. Tap **Scan** (~15–20 s; your phone drops briefly while the single radio scans,
   then reconnects automatically and the network table appears).
5. Tap an SSID row to expand the devices seen behind it.
6. Select rows → **Attack** to target them, or **Protect** to mark them SAFE so
   God Mode skips them.
7. **God Mode** attacks everything else; **Stop** ends any mode and restores the
   control AP so your phone reconnects.
8. The status line shows the live injection rate; the **Sent** column counts frames
   per network.

The same actions exist over USB serial at 115200 baud:

```
scan                                   # dual-band scan + client sniff
list                                   # networks as JSON (index, ssid, band, ch, bssid, sec, safe, clients…)
deauth <i> [i...]                      # attack AP(s): broadcast + directed to their sniffed clients
strike <ch> <is5> <bssid> <client-mac> # lock ONE known client (parked, directed — best kill)
hunt <mac> <ch> <is5> <bssid> [...]    # chase one client across up to 4 bands
god                                    # attack every non-SAFE network, auto re-scan every 45 s
protect <i> [i...]                     # toggle SAFE flag on network index(es)
dev                                    # count stations per AP
stop                                   # stop; restores AP + STA
heap                                   # free/min heap
txtest                                 # harmless injection self-test (fake BSSID)
```

Example — strike one known client on 2.4 GHz channel 11:

```
strike 11 0 DD:EE:FF:44:55:66 AA:BB:CC:11:22:33
```

(Addresses shown are placeholders — substitute your own lab targets.)

## Repository layout

```
README.md                  this file
docs/architecture.md       firmware architecture and data flow
docs/flashing_guide.md     detailed flash/setup instructions
docs/usage_guide.md        web UI walkthrough + full CLI/API reference
firmware/                  ESP-IDF project (the entire product lives here)
firmware/prebuilt/         ready-to-flash merged image + checksum
```

More detail: [docs/architecture.md](docs/architecture.md),
[docs/flashing_guide.md](docs/flashing_guide.md),
[docs/usage_guide.md](docs/usage_guide.md).

## Safety & legal

- Run attacks only against networks you own or are explicitly permitted to test
  (your own lab, your own router, an engagement with written authorization).
- Deauthentication disrupts **every client** of the targeted AP, not just yours.
- Mark your own infrastructure **SAFE** before using God Mode.
- No configuration persists across reboots (RAM-only): the device always boots idle
  and never attacks until you explicitly start a mode.

## License & warranty

Personal research project; provided as-is, no warranty. You are responsible for
complying with the laws of your jurisdiction.
