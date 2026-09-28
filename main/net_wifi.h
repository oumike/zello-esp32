#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Brings up the ESP32-C6 coprocessor (ESP-Hosted over SDIO) and the STA
// interface. Safe to call with an empty SSID; connect later via net_wifi_connect.
esp_err_t net_wifi_init(void);
esp_err_t net_wifi_connect(const char *ssid, const char *pass);
bool net_wifi_wait_ip(uint32_t timeout_ms);

typedef struct {
    char ssid[33];
    int8_t rssi;
    bool secure;  // false for an open network
} net_wifi_ap_t;

// Blocking scan. Fills `out` with up to `max` networks, strongest first, one
// entry per SSID and hidden networks skipped. Returns the count, or -1.
int net_wifi_scan(net_wifi_ap_t *out, int max);

typedef enum {
    NET_JOIN_OK,
    NET_JOIN_BAD_PASSWORD,
    NET_JOIN_NOT_FOUND,
    NET_JOIN_TIMEOUT,
    NET_JOIN_FAILED,
} net_join_result_t;

// Setup hotspot (AP+STA), used during onboarding for the web config. SSID is
// "Scheff-XXXX" (last MAC bytes), open (no password).
// The AP's own address is 192.168.4.1.
esp_err_t net_wifi_ap_start(void);
void net_wifi_ap_stop(void);
bool net_wifi_ap_active(void);
const char *net_wifi_ap_ssid(void);

// Station IP as dotted text into `buf`; false if not connected.
bool net_wifi_sta_ip(char *buf, size_t size);

// Blocking connect that reports how it ended, for the setup UI. On success
// the link stays up and reconnects as net_wifi_connect() does; on failure
// reconnect attempts stop.
net_join_result_t net_wifi_join(const char *ssid, const char *pass, uint32_t timeout_ms);
bool net_wifi_has_ip(void);
void net_wifi_print_status(void);

// Waits for SNTP to set the clock. TLS rejects a certificate it cannot date, so
// the first wss:// connection has to come after this. Returns false on timeout,
// which only means the handshake may need a retry.
bool net_wifi_wait_time(uint32_t timeout_ms);
