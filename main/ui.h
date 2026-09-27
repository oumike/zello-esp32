#pragma once

// Touch UI. Call after display_init(), net_wifi_init() and channel_list_init().
// Boots into Wi-Fi setup when no network is saved, then the Zello account, then
// the channel picker; once all three are done it stays on the home screen.
void ui_start(void);
