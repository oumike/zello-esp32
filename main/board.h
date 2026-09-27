#pragma once

// LilyGO T-Display-P4 V1.0 board support: the two I2C buses, the XL9535 I/O
// expander rails that gate audio / Wi-Fi / SD / display power, and the fuel
// gauge.

#include <stdbool.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "driver/i2c_master.h"

// I2C bus 1: XL9535 I/O expander, touch, RTC, fuel gauge.
#define BOARD_I2C1_SDA 7
#define BOARD_I2C1_SCL 8
// I2C bus 2: ES8311 codec, IMU, haptics.
#define BOARD_I2C2_SDA 20
#define BOARD_I2C2_SCL 21

// ES8311 I2S (port 0)
#define BOARD_I2S_MCLK 13
#define BOARD_I2S_BCLK 12
#define BOARD_I2S_WS   9
#define BOARD_I2S_DOUT 10  // P4 -> codec DAC
#define BOARD_I2S_DIN  11  // codec ADC -> P4
#define BOARD_ES8311_ADDR 0x18

// BOOT button, active low. Shared with the Ethernet RMII TXD1, so don't use
// it as PTT if Ethernet is ever enabled.
#define BOARD_BOOT_BUTTON 35

esp_err_t board_init(void);

// Shared OUT_5V audio domain: NS4150B speaker amp and the ES8311 analog supply.
esp_err_t board_audio_power(bool on);

// ESP32-C6 coprocessor EN line (active high).
esp_err_t board_c6_enable(bool on);

// microSD card supply (XL9535 IO15, active low).
esp_err_t board_sd_power(bool on);

// RM69A10 AMOLED and GT9895 touch reset lines (XL9535). false holds the part
// in reset, true releases it.
esp_err_t board_screen_reset_release(bool released);
esp_err_t board_touch_reset_release(bool released);

// Reads the BQ27220 state of charge (0-100%).
esp_err_t board_battery_percent(uint8_t *percent);

// I2C bus 1 (expander, touch) and bus 2 (codec).
i2c_master_bus_handle_t board_main_i2c_bus(void);
i2c_master_bus_handle_t board_codec_i2c_bus(void);
