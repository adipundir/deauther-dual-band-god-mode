# Usage guide

Everything the deauther can do, from both control surfaces: the on-device web UI
and the serial console. Both drive the same underlying commands.

> Authorized testing only — your own networks and devices, or engagements with
> written permission.

## Web UI

Join the `deauther-ctrl` access point (default password `deauther1234`, set in
`firmware/wifi/wifi_controller.h`) and open `http://192.168.4.1/`.

### Workflow

1. **Scan** — runs a dual-band scan plus client discovery (~15–20 s). Your phone
   drops while the single radio scans; the page reconnects automatically and
   renders the table.
2. **Table columns** — select box · SSID (tap to expand its device list) · band ·
   security · channel · signal bars + dBm · device count (`Dev`) · frames sent
   (`Sent`, live) · BSSID. Badges: `[PMF]` = WPA3/Protected Management Frames,
   immune to deauth; `[SAFE]` = protected from God Mode.
3. **Select rows → Attack** — continuous broadcast + directed deauth against the
   selected APs until you tap Stop. The status line shows the live pkt/s rate.
   PMF-selected rows are noted as un-attackable.
4. **Protect** — toggles SAFE on selected rows (keyed by BSSID, survives re-scans).
5. **God Mode** — attacks every network in range except SAFE ones and the device's
   own AP; re-scans every 45 s to add newcomers and drop vanished networks.
6. **Stop** — ends any mode and re-hosts the control AP so your phone reconnects.

The header dot/accent bar mirrors the LED state; polling is every 2 s when not
attacking.

### HTTP API

All endpoints are GET and return JSON. The UI drives scan/list/status/attack/
protect/god/stop; `/api/devices` backs the per-AP device expander and
`/api/clients` is available for scripted use.

| Endpoint | Response | Effect |
|---|---|---|
| `/` | HTML | The control UI |
| `/api/scan` | `{"count":n}` | Dual-band scan + client sniff (blocking ~15–20 s) |
| `/api/list` | JSON array | Current network table |
| `/api/devices?idx=i` | JSON array | Station MACs/RSSI/pkts for network `i` |
| `/api/status` | `{"mode":..,"rate":..}` | Mode string + frames/sec |
| `/api/attack?sel=0,2` | `{"armed":n}` | Start TARGET attack on listed indices |
| `/api/protect?sel=0,2` | `{"ok":true}` | Toggle SAFE on listed indices |
| `/api/god` | `{"ok":true}` | Start God Mode |
| `/api/stop` | `{"ok":true}` | Stop and restore the AP |
| `/api/clients` | `{"total":n}` | Total stations found by the last sniff |

## Serial CLI

UART0 @ 115200 (e.g. `idf.py monitor`). Commands:

```
scan                                   # dual-band scan + client sniff
list                                   # networks as JSON
dev                                    # count stations per AP
deauth <i> [i...]                      # attack selected APs
strike <ch> <is5> <bssid> <mac>        # parked directed kill of one known client
hunt <mac> <ch> <is5> <bssid> [...]    # chase one client across up to 4 bands
god                                    # GOD MODE
protect <i> [i...]                     # toggle SAFE flag
stop                                   # stop attacking
heap                                   # free/min heap
txtest                                 # harmless TX self-test (fake BSSID)
help                                   # command list
```

Examples:

```
# Directed kill of one known client (2.4 GHz ch 11):
strike 11 0 DD:EE:FF:44:55:66 AA:BB:CC:11:22:33

# Same but 5 GHz channel 157:
strike 157 1 DD:EE:FF:44:55:67 AA:BB:CC:11:22:33

# Chase a band-steering laptop across both of its BSSIDs:
hunt AA:BB:CC:11:22:33 11 0 DD:EE:FF:44:55:66 157 1 DD:EE:FF:44:55:67
```

(MACs above are placeholders — use targets from your own lab scan.)

## LED codes

| Color | Meaning |
|---|---|
| Dim white, steady | Idle |
| Blue, steady | Scanning |
| Cyan, steady | Counting devices |
| Red, blinking | Attacking (target/strike/hunt) |
| Rainbow strobe | God Mode |

## Status log

While attacking, the serial log prints once per second:
frames/sec, total sent, fail %, heap, and per-mode detail (target clients with
RSSI/frame counts, strike/hunt radio channel, god-mode totals).

## Reading results

- **Directed vs broadcast:** modern clients ignore broadcast deauth; the directed
  frames (to their real MAC) are what actually disconnect them. That's why the
  device sniffs clients before and during attacks.
- **Sent climbing but nothing drops?** The target may be PMF/WPA3 (check `[PMF]`),
  or a band-steerer hopping to the other band of the same router — try `hunt`
  across both bands, or pin the client to one band first.
- **Fail % high** usually means the TX interface/channel mismatch was corrected by
  parking; persistent high fail with zero effect suggests the target isn't hearing
  you (range/DFS channel).
