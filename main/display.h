#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "esp_err.h"

// RM69A10 4.1" AMOLED (568x1232 portrait, MIPI-DSI).
#define DISPLAY_H_RES 568
#define DISPLAY_V_RES 1232

esp_err_t display_init(void);

// LVGL is not thread safe: hold this around any lv_* call made outside LVGL's
// own callbacks (which already run with it held).
bool display_lock(void);
void display_unlock(void);

// Panel brightness, normalized to 0-255.
void display_set_brightness(uint8_t level);
