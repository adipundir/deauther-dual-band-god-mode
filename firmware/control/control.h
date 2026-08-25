// Shared control surface — driven by both the web UI and the serial CLI.
#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>

void        control_init(void);                     // start the attack task
int         control_scan(void);                     // scan; returns AP count
int         control_count_clients(void);            // sniff stations; returns total
int         control_list_json(char *out, int cap);  // JSON array; returns length
int         control_devices_json(int idx, char *out, int cap); // MACs of one AP
int         control_attack_selection(const int *idx, int n); // # armed; -1 none valid
int         control_strike(uint8_t ch, int is5, const uint8_t bssid[6], const uint8_t mac[6]); // lock 1 client
// Dual-band hunt: chase one client MAC across up to 4 (bssid,ch,is5) band entries.
int         control_hunt(const uint8_t mac[6], const uint8_t (*bssids)[6],
                         const uint8_t *chans, const int *is5, int n);
void        control_protect(const int *idx, int n);          // toggle God-Mode protection
void        control_god(void);
void        control_stop(void);
const char *control_mode_str(void);

// LED status code: 0 idle, 1 scanning, 2 counting devices, 3 attacking, 4 god.
int         control_led_code(void);

// Live transmit rate in frames/sec (0 when idle).
uint32_t    control_rate(void);

#endif // CONTROL_H
