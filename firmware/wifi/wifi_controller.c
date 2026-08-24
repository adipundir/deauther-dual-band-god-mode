/*
 * WiFi Controller — ESP32-C5 dual-band deauther.
 * AP+STA: the AP hosts the phone's control UI; the STA scans; promiscuous
 * sniffing counts stations per AP. No client connection is ever made.
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi_controller.h"

static const char *TAG = "WIFI_CTRL";

static wifi_network_t s_networks[WIFI_MAX_NETWORKS];
static int s_network_count = 0;

// Per-AP station records (parallel to s_networks).
static wifi_client_t s_clients[WIFI_MAX_NETWORKS][WIFI_MAX_CLIENTS];

// Protected BSSIDs (God Mode skips these), kept across rescans.
static uint8_t s_protected[WIFI_MAX_NETWORKS][6];
static int s_prot_count = 0;

bool wifi_is_protected(const uint8_t bssid[6])
{
    for (int i = 0; i < s_prot_count; i++)
        if (memcmp(s_protected[i], bssid, 6) == 0) return true;
    return false;
}

static void sync_safe(const uint8_t bssid[6], bool v)
{
    for (int i = 0; i < s_network_count; i++)
        if (memcmp(s_networks[i].bssid, bssid, 6) == 0) s_networks[i].safe = v;
}

bool wifi_toggle_protected(const uint8_t bssid[6])
{
    for (int i = 0; i < s_prot_count; i++) {
        if (memcmp(s_protected[i], bssid, 6) == 0) {           // present -> remove
            memmove(s_protected[i], s_protected[s_prot_count - 1], 6);
            s_prot_count--;
            sync_safe(bssid, false);
            return false;
        }
    }
    if (s_prot_count < WIFI_MAX_NETWORKS) memcpy(s_protected[s_prot_count++], bssid, 6);
    sync_safe(bssid, true);
    return true;
}

void wifi_system_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    // "US" unlocks 5 GHz UNII-3 (ch 149-165) that worldwide "01" blocks, so we
    // can sniff/inject on the high channels many routers use. (Your hardware,
    // your call — this is a research tool.)
    ESP_ERROR_CHECK(esp_wifi_set_country_code("US", true));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    wifi_config_t ap = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = sizeof(WIFI_AP_SSID) - 1,
            .password = WIFI_AP_PASS,
            .channel = WIFI_AP_HOME_CH,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_band_mode(WIFI_BAND_MODE_AUTO));
    esp_wifi_set_max_tx_power(84);   // max (~20 dBm) so deauth frames reach farther

    ESP_LOGI(TAG, "WiFi up — AP '%s' on ch%d, dual-band STA scan",
             WIFI_AP_SSID, WIFI_AP_HOME_CH);
}

int wifi_dual_band_scan(void)
{
    // The C5 calibrates its 5 GHz radio on first use; without this the very
    // first scan after boot returns 2.4 GHz only. Touch a 5 GHz channel to warm
    // it up, then scan both bands.
    esp_wifi_set_band_mode(WIFI_BAND_MODE_5G_ONLY);
    esp_wifi_set_channel(36, WIFI_SECOND_CHAN_NONE);
    vTaskDelay(pdMS_TO_TICKS(30));
    esp_wifi_set_band_mode(WIFI_BAND_MODE_AUTO);
    vTaskDelay(pdMS_TO_TICKS(10));

    wifi_scan_config_t scan = {
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = { .active = { .min = 40, .max = 120 } },
    };
    ESP_LOGI(TAG, "Scanning 2.4 + 5 GHz ...");
    esp_err_t err = esp_wifi_scan_start(&scan, true);
    if (err != ESP_OK) { ESP_LOGE(TAG, "scan_start: %s", esp_err_to_name(err)); return -1; }

    uint16_t found = WIFI_MAX_NETWORKS;
    static wifi_ap_record_t recs[WIFI_MAX_NETWORKS];
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&found, recs));

    s_network_count = 0;
    for (int i = 0; i < found && s_network_count < WIFI_MAX_NETWORKS; i++) {
        wifi_network_t *n = &s_networks[s_network_count++];
        memcpy(n->bssid, recs[i].bssid, 6);
        strncpy(n->ssid, (const char *)recs[i].ssid, sizeof(n->ssid) - 1);
        n->ssid[sizeof(n->ssid) - 1] = '\0';
        n->rssi = recs[i].rssi;
        n->channel = recs[i].primary;
        n->is_5ghz = (recs[i].primary > 14);
        // WPA3 / mixed WPA2-WPA3 imply Protected Management Frames -> deauth-immune.
        n->auth = recs[i].authmode;
        n->prot = (recs[i].authmode == WIFI_AUTH_WPA3_PSK ||
                   recs[i].authmode == WIFI_AUTH_WPA2_WPA3_PSK ||
                   recs[i].authmode == WIFI_AUTH_WPA3_ENT_192);
        n->clients = 0;
        n->tx = 0;
        n->safe = wifi_is_protected(n->bssid);   // carry protection across rescans
    }
    ESP_LOGI(TAG, "Scan done: %d APs", s_network_count);
    return s_network_count;
}

int wifi_get_networks(const wifi_network_t **out)
{
    if (out) *out = s_networks;
    return s_network_count;
}

void wifi_set_channel_band(uint8_t channel, bool is_5ghz)
{
    esp_wifi_set_band_mode(is_5ghz ? WIFI_BAND_MODE_5G_ONLY : WIFI_BAND_MODE_2G_ONLY);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

void wifi_restore_ap_channel(void)
{
    esp_wifi_set_band_mode(WIFI_BAND_MODE_AUTO);
    esp_wifi_set_channel(WIFI_AP_HOME_CH, WIFI_SECOND_CHAN_NONE);
}

void wifi_park_channel(uint8_t ch, bool is_5ghz)
{
    // Hold the radio on ONE channel by reconfiguring the AP to live there. A
    // 2.4 GHz AP can't hold the radio on a 5 GHz channel — in APSTA the driver
    // keeps snapping back to ch1 to beacon, so injected 5 GHz frames scatter and
    // never reach the target. Making the AP a 5 GHz AP on `ch` parks the radio
    // so esp_wifi_80211_tx(WIFI_IF_AP) actually radiates on `ch`. Phone control
    // drops while parked off ch1 — fine, we don't control attacks from the phone.
    esp_wifi_set_band_mode(is_5ghz ? WIFI_BAND_MODE_5G_ONLY : WIFI_BAND_MODE_2G_ONLY);
    wifi_config_t c;
    if (esp_wifi_get_config(WIFI_IF_AP, &c) == ESP_OK) {
        c.ap.channel = ch;
        esp_wifi_set_config(WIFI_IF_AP, &c);   // AP re-hosts on ch (this band)
    }
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

void wifi_set_ap_channel(uint8_t ch)
{
    // Band mode FIRST — setting a 2.4 GHz channel while still in 5G_only errors.
    esp_wifi_set_band_mode(WIFI_BAND_MODE_AUTO);
    wifi_config_t c;
    if (esp_wifi_get_config(WIFI_IF_AP, &c) != ESP_OK) return;
    c.ap.channel = ch;
    esp_wifi_set_config(WIFI_IF_AP, &c);   // AP re-hosts on the new channel
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

// ---- station sniffing -------------------------------------------------------

// Record/update station `mac` under network index `ni`.
static void add_client(int ni, const uint8_t *mac, int8_t rssi)
{
    for (int c = 0; c < s_networks[ni].clients; c++) {
        if (memcmp(s_clients[ni][c].mac, mac, 6) == 0) {     // seen -> update
            s_clients[ni][c].rssi = rssi;
            if (s_clients[ni][c].pkts < 0xffff) s_clients[ni][c].pkts++;
            return;
        }
    }
    if (s_networks[ni].clients >= WIFI_MAX_CLIENTS) return;
    wifi_client_t *nc = &s_clients[ni][s_networks[ni].clients];
    memcpy(nc->mac, mac, 6);
    nc->rssi = rssi;
    nc->pkts = 1;
    s_networks[ni].clients++;
}

static volatile uint32_t s_sniff_seen = 0;   // debug: frames delivered to cb

static void sniffer_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    s_sniff_seen++;
    if (type != WIFI_PKT_DATA && type != WIFI_PKT_MGMT) return;
    const wifi_promiscuous_pkt_t *p = (wifi_promiscuous_pkt_t *)buf;
    const uint8_t *f = p->payload;
    uint8_t b1 = f[1];
    bool tods = b1 & 0x01, fromds = b1 & 0x02;
    const uint8_t *a1 = f + 4, *a2 = f + 10;   // addr1, addr2
    const uint8_t *bssid, *station;
    if (!tods && fromds)      { bssid = a2; station = a1; }  // AP -> STA
    else if (tods && !fromds) { bssid = a1; station = a2; }  // STA -> AP
    else return;                                             // skip other

    if (station[0] & 0x01) return;   // ignore multicast/broadcast "stations"

    for (int i = 0; i < s_network_count; i++)
        if (memcmp(s_networks[i].bssid, bssid, 6) == 0) {
            add_client(i, station, p->rx_ctrl.rssi);
            return;
        }
}

int wifi_count_clients(void)
{
    if (s_network_count == 0) return 0;

    for (int i = 0; i < s_network_count; i++) s_networks[i].clients = 0;

    // Distinct (channel, band) pairs to visit.
    struct { uint8_t ch; bool g5; } chans[WIFI_MAX_NETWORKS];
    int nch = 0;
    for (int i = 0; i < s_network_count; i++) {
        bool seen = false;
        for (int c = 0; c < nch; c++)
            if (chans[c].ch == s_networks[i].channel && chans[c].g5 == s_networks[i].is_5ghz) { seen = true; break; }
        if (!seen) { chans[nch].ch = s_networks[i].channel; chans[nch].g5 = s_networks[i].is_5ghz; nch++; }
    }

    esp_wifi_set_promiscuous_rx_cb(&sniffer_cb);
    wifi_promiscuous_filter_t filt = { .filter_mask = WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_MGMT };
    esp_wifi_set_promiscuous_filter(&filt);
    esp_wifi_set_promiscuous(true);
    vTaskDelay(pdMS_TO_TICKS(80));   // let promiscuous settle before first hop
    s_sniff_seen = 0;

    for (int c = 0; c < nch; c++) {
        wifi_set_channel_band(chans[c].ch, chans[c].g5);
        vTaskDelay(pdMS_TO_TICKS(15));    // let the hop take before counting
        uint32_t before = s_sniff_seen;
        vTaskDelay(pdMS_TO_TICKS(700));   // dwell to catch traffic (kept short)
        uint8_t got = 0; wifi_second_chan_t sec;
        esp_wifi_get_channel(&got, &sec);
        ESP_LOGI(TAG, "sniff ch req=%u got=%u (%s): %lu frames",
                 chans[c].ch, got, chans[c].g5 ? "5G" : "2.4G",
                 (unsigned long)(s_sniff_seen - before));
    }

    esp_wifi_set_promiscuous(false);
    wifi_restore_ap_channel();
    ESP_LOGI(TAG, "sniff total frames=%lu", (unsigned long)s_sniff_seen);

    int total = 0;
    for (int i = 0; i < s_network_count; i++) total += s_networks[i].clients;
    ESP_LOGI(TAG, "Client sniff: %d stations across %d channels", total, nch);
    return total;
}

void wifi_add_tx(int ni, uint32_t n)
{
    if (ni >= 0 && ni < s_network_count) s_networks[ni].tx += n;
}

void wifi_promisc_discovery(bool on)
{
    if (on) {
        esp_wifi_set_promiscuous_rx_cb(&sniffer_cb);
        wifi_promiscuous_filter_t f = { .filter_mask = WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_MGMT };
        esp_wifi_set_promiscuous_filter(&f);
        esp_wifi_set_promiscuous(true);
    } else {
        esp_wifi_set_promiscuous(false);
    }
}

void wifi_sniff_channel(uint8_t channel, bool is_5ghz, int dwell_ms)
{
    esp_wifi_set_promiscuous_rx_cb(&sniffer_cb);
    wifi_promiscuous_filter_t f = { .filter_mask = WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_MGMT };
    esp_wifi_set_promiscuous_filter(&f);
    esp_wifi_set_promiscuous(true);
    wifi_set_channel_band(channel, is_5ghz);
    vTaskDelay(pdMS_TO_TICKS(dwell_ms));      // let clients transmit so we see them
    esp_wifi_set_promiscuous(false);
}

int wifi_get_clients(int ni, wifi_client_t *out, int cap)
{
    if (ni < 0 || ni >= s_network_count) return 0;
    int c = s_networks[ni].clients;
    if (c > cap) c = cap;
    for (int i = 0; i < c; i++) out[i] = s_clients[ni][i];
    return c;
}
