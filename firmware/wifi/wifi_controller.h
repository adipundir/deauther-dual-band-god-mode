// WiFi controller — dual-band init, scan, channel control, client sniffing.
#ifndef WIFI_CONTROLLER_H
#define WIFI_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

#define WIFI_MAX_NETWORKS 64
#define WIFI_MAX_CLIENTS  16   // tracked stations per AP

// Control access point the phone connects to. Discreet name; strong WPA2 pass.
#define WIFI_AP_SSID     "deauther-ctrl"
#define WIFI_AP_PASS     "deauther1234"          // default; change before flashing (8-63 chars, WPA2)
#define WIFI_AP_HOME_CH  1                       // 2.4 GHz home channel

typedef struct {
    uint8_t  bssid[6];
    char     ssid[33];
    int8_t   rssi;
    uint8_t  channel;
    bool     is_5ghz;
    bool     prot;      // WPA3/PMF — ignores deauth
    uint8_t  auth;      // wifi_auth_mode_t (security standard) frames
    uint8_t  clients;   // stations seen associated (after wifi_count_clients)
    uint32_t tx;        // deauth frames sent to this AP this session
    bool     safe;      // user-protected: God Mode skips it
} wifi_network_t;

typedef struct {
    uint8_t  mac[6];
    int8_t   rssi;      // last seen signal
    uint16_t pkts;      // frames observed
} wifi_client_t;

void wifi_system_init(void);
int  wifi_dual_band_scan(void);
int  wifi_get_networks(const wifi_network_t **out);
void wifi_set_channel_band(uint8_t channel, bool is_5ghz);
void wifi_restore_ap_channel(void);

// Park the radio on ONE channel/band by re-hosting the AP there, so injection
// via the AP interface actually radiates on that channel. Required for 5 GHz
// (a 2.4 GHz AP can't hold the radio on 5 GHz). Phone control drops while parked.
void wifi_park_channel(uint8_t ch, bool is_5ghz);

// Move the control AP back to a 2.4 GHz home channel (2.4 GHz). Used by Stop
// so the phone can reconnect after an attack parked it elsewhere.
void wifi_set_ap_channel(uint8_t ch);

// Hop every scanned channel, sniff for associated stations, fill .clients.
// Returns the total number of distinct stations found. Drops the AP while it
// runs (single radio), then restores it.
int  wifi_count_clients(void);

// Copy up to `cap` station records seen for network `ni`. Returns count.
int  wifi_get_clients(int ni, wifi_client_t *out, int cap);

// Add `n` to the deauth-frames-sent tally for network `ni` (for the UI).
void wifi_add_tx(int ni, uint32_t n);

// Turn promiscuous client-discovery on/off (used during an attack so we keep
// learning the target's client MACs to hit them with directed deauth).
void wifi_promisc_discovery(bool on);

// Camp on one channel with promiscuous ON for `dwell_ms` to capture the clients
// (station MACs) of any AP on it. Used before an attack so directed deauth has
// real client MACs — modern devices ignore broadcast deauth.
void wifi_sniff_channel(uint8_t channel, bool is_5ghz, int dwell_ms);

// Protected-networks list (God Mode skips these). Keyed by BSSID so it survives
// rescans. wifi_toggle_protected returns the new state.
bool wifi_toggle_protected(const uint8_t bssid[6]);
bool wifi_is_protected(const uint8_t bssid[6]);

#endif // WIFI_CONTROLLER_H
