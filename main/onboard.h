#pragma once

// Onboarding state shared by the touch UI and the web config: Wi-Fi first,
// then the Zello account, then a channel. Either side can complete a step; the
// other follows along through the change callbacks.

#include <stdbool.h>
#include "esp_err.h"

bool onboard_wifi_done(void);
bool onboard_account_done(void);
bool onboard_channel_done(void);
static inline bool onboard_complete(void)
{
    return onboard_wifi_done() && onboard_account_done() && onboard_channel_done();
}

// Stores a network that was just joined successfully and schedules a reboot.
esp_err_t onboard_save_wifi(const char *ssid, const char *pass);

// Validates, saves and applies the account. `network` is "consumer" or "work";
// `token` is the developer JWT (required on the consumer network) and
// `work_network` the Zello Work network name (required on that one). A blank
// password or token keeps whatever is stored, so the web config never has to
// send either back. Returns ESP_ERR_INVALID_ARG with `err` filled in when
// something needed is missing.
esp_err_t onboard_save_account(const char *network, const char *username, const char *password, const char *token,
                               const char *work_network, const char **err);

// Saves the channel to join and reconnects.
esp_err_t onboard_save_channel(const char *channel, const char **err);

// Refreshes onboarding consumers after settings are restored in bulk.
void onboard_settings_changed(void);

// Called (from the task that made the change) after any step is saved.
void onboard_set_ui_cb(void (*cb)(void));
void onboard_set_web_cb(void (*cb)(void));
