# Firmware architecture

How the ESP32-C5 deauther firmware is put together: modules, tasks, data flow, and
the reasoning behind the trickier design decisions.

## Modules

```
app_main ──► wifi_system_init()   radio up: AP (control UI) + STA (scan), dual band
         ├─► control_init()       attack + stats FreeRTOS tasks
         ├─► led_init()           WS2812 status LED task
         ├─► http_server_start()  single-page web UI at http://192.168.4.1/
         └─► start_cli()          esp_console REPL on UART0
```

| Module | Responsibility |
|---|---|
| `wifi/` | Radio lifecycle: AP+STA bring-up, dual-band scan (`wifi_dual_band_scan`), channel parking (`wifi_park_channel`), promiscuous station sniffing (`wifi_count_clients`, `wifi_sniff_channel`), SAFE/protected list |
| `deauth/` | Frame template, `blast()` raw-TX loop with NO_MEM retry, TX interface selection, per-second stats; linker-wrap bypass of the deauth sanity check |
| `control/` | The only owner of attack state. Modes: `IDLE`, `TARGET` (selected APs), `STRIKE` (one locked client), `HUNT` (one client across bands), `GOD`. Both the HTTP handlers and CLI commands call into it — there is exactly one code path per action |
| `godmode/` | God Mode policy on top of control/wifi primitives (skip SAFE + own AP, focus-fire client-bearing networks, fallback broadcast sweep) |
| `http_server/` | Serves the embedded single-page UI and a small JSON API |
| `led/` | Maps `control_led_code()` to colors/blinks |

## Tasks

| Task | Prio | Stack | Loop |
|---|---|---|---|
| `attack` | 4 | 4 KB | Runs the active mode's cycle: TARGET hammer / STRIKE directed blast / HUNT band rotation / GOD sweep + 45 s re-scan. Sleeps 200 ms when idle |
| `stats` | 3 | 3 KB | Once per second: frames/sec, fail %, heap, per-mode detail log |
| `led` | 3 | 2.5 KB | Polls `control_led_code()` every ~50–400 ms |
| `httpd` (IDF) | — | — | Serves UI/API; long calls (scan) block one worker by design — the UI retries |
| WiFi driver task | high | — | Delivers promiscuous frames to `sniffer_cb`, which records stations |

## Data flow

1. **Scan** — `wifi_dual_band_scan()` warms the 5 GHz radio, runs an active
   scan across both bands, and rebuilds the network table (BSSID, SSID, RSSI,
   channel, band, auth). WPA3/WPA2-WPA3-mixed are flagged PMF = deauth-immune.
   `wifi_count_clients()` then hops every distinct (channel, band) pair with
   promiscuous mode on, dwelling ~700 ms each; data/mgmt frames map
   addr1/addr2 → (BSSID, station) pairs that fill each AP's client list.
2. **Attack (TARGET)** — snapshots the selected networks first so later re-scans
   can't shift indices mid-attack, pre-sniffs each target's channel for clients,
   then loops: broadcast deauth+disassoc burst plus directed bursts per known
   client. Every ~12 cycles it re-sniffs target channels to catch clients that
   started transmitting while under attack.
3. **STRIKE** — parks the whole radio on one (channel, band), drops the STA so
   nothing pulls the radio off-channel, and blasts directed deauth at one MAC.
4. **HUNT** — rotates across up to four parked bands, hammering ~300 ms each, so a
   client roaming between bands lands where the attack already is.
5. **GOD** — every 45 s refreshes scan+clients, otherwise sweeps all non-SAFE
   networks with detected clients (broadcast + heavy directed); if no clients were
   seen anywhere it falls back to a broadcast sweep.

## Design notes

- **Deauth TX gating** — Espressif gates raw deauth/disassoc in
  `ieee80211_raw_frame_sanity_check()`. Overridden via linker wrap
  (`-Wl,-wrap=...` in `main/CMakeLists.txt`, stub in `deauth/deauth_engine.c`);
  the IDF library itself is never modified.
- **Channel parking** — in APSTA the AP beacons pull the radio back to its config
  channel, scattering injected 5 GHz frames. `wifi_park_channel()` re-hosts the AP
  on the target channel/band so `esp_wifi_80211_tx(WIFI_IF_AP)` truly radiates
  there. Control drops while parked; `stop` re-hosts the AP home.
- **Concurrency** — attack state is written only after the attack task is halted
  (`MODE_IDLE` + settle delay) so a re-arm can't feed it torn structures. Client
  records, shared between the WiFi driver task (writer) and everything else
  (readers), are guarded by a spinlock with microsecond-scale critical sections.
- **No persistence** — configuration is RAM-only by design: the device boots idle
  and never attacks until someone explicitly starts a mode.
- **SAFE list** — BSSID-keyed so protection survives re-scans; God Mode also always
  skips its own control AP.
