#pragma once

#include "esp_err.h"

// Browser-based setup (Wi-Fi, the Zello account and channels) on port 80. Until
// onboarding is complete it also brings up a board-named open setup hotspot,
// with the page at http://192.168.4.1 (no captive portal). The hotspot turns
// off as soon as station Wi-Fi gets an IP; the web server itself remains
// available there. Call after net_wifi_init().
esp_err_t web_config_start(void);
