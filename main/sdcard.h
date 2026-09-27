#pragma once

#include <stdbool.h>
#include "esp_err.h"

#define SDCARD_MOUNT "/sdcard"

// Mounts the board's microSD card at /sdcard if it isn't already. Safe to call
// repeatedly: a card inserted after boot is picked up on the next call.
esp_err_t sdcard_mount(void);
bool sdcard_mounted(void);
