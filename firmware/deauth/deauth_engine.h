// Deauthentication engine — builds and injects real 802.11 deauth frames.
#ifndef DEAUTH_ENGINE_H
#define DEAUTH_ENGINE_H

#include <stdint.h>
#include "esp_wifi_types.h"
#include "wifi_controller.h"

// Select the interface deauth frames inject through. 5 GHz needs WIFI_IF_STA
// (the AP is 2.4 GHz-only and can't transmit on 5 GHz); 2.4 GHz uses WIFI_IF_AP.
void deauth_set_tx_if(wifi_interface_t ifx);

// Send `count` deauth frames from `bssid` to `dest` (use the broadcast helper
// to hit every client of an AP). Assumes the radio is already on the right
// channel/band. Returns 0 on success, negative on error.
int deauth_send(const uint8_t bssid[6], const uint8_t dest[6],
                uint16_t reason, int count);

// Hop to the network's channel/band and blast it with deauth frames to
// broadcast (kicks every associated client). Returns 0 on success.
int deauth_attack_network(const wifi_network_t *net, int burst);

// Directed unicast deauth to one client of an AP (both directions).
int deauth_client(const uint8_t bssid[6], const uint8_t client[6], int burst);

// Cumulative TX counters, for the live packets/sec readout.
void deauth_get_stats(uint32_t *ok, uint32_t *fail);

#endif // DEAUTH_ENGINE_H
