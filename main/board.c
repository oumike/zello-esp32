#include "board.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "board";

// XL9535 (PCA9535-compatible) 16-bit I/O expander.
#define XL9535_ADDR     0x20
#define XL9535_REG_IN0  0x00
#define XL9535_REG_OUT0 0x02
#define XL9535_REG_CFG0 0x06  // 1 = input

#define BQ27220_ADDR                    0x55
#define BQ27220_REG_STATE_OF_CHARGE     0x2c

// Expander pin numbers: IO0..IO7 = 0..7, IO10..IO17 = 8..15.
#define XIO_POWER_EN_3V3  0
#define XIO_SKY13453_VCTL 1
#define XIO_SCREEN_RST    2
#define XIO_TOUCH_RST     3
#define XIO_ETH_RST       5
#define XIO_AUDIO_PWR_EN  6
#define XIO_USB_PHY_PWR   8   // IO10
#define XIO_C6_EN         12  // IO14
#define XIO_SD_PWR_N      13  // IO15, active low

static i2c_master_bus_handle_t s_i2c1, s_i2c2;
static i2c_master_dev_handle_t s_xl9535, s_bq27220;
static SemaphoreHandle_t s_xl_lock;
static uint16_t s_out, s_cfg;  // shadow registers

static esp_err_t xl_write16(uint8_t reg, uint16_t v)
{
    uint8_t buf[3] = {reg, v & 0xff, v >> 8};
    return i2c_master_transmit(s_xl9535, buf, sizeof(buf), 100);
}

static esp_err_t xl_read16(uint8_t reg, uint16_t *v)
{
    uint8_t b[2];
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(s_xl9535, &reg, 1, b, 2, 100), TAG, "xl9535 read");
    *v = b[0] | (b[1] << 8);
    return ESP_OK;
}

// Drive an expander pin as an output at `level`. The output latch is written
// before the direction so the pin never glitches.
static esp_err_t xl_set(int pin, bool level)
{
    xSemaphoreTake(s_xl_lock, portMAX_DELAY);
    uint16_t out = level ? (s_out | (1u << pin)) : (s_out & ~(1u << pin));
    esp_err_t err = xl_write16(XL9535_REG_OUT0, out);
    if (err == ESP_OK) {
        s_out = out;
        if (s_cfg & (1u << pin)) {
            uint16_t cfg = s_cfg & ~(1u << pin);
            err = xl_write16(XL9535_REG_CFG0, cfg);
            if (err == ESP_OK) s_cfg = cfg;
        }
    }
    xSemaphoreGive(s_xl_lock);
    return err;
}

static esp_err_t new_bus(int port, int sda, int scl, i2c_master_bus_handle_t *out)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port = port,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&cfg, out);
}

esp_err_t board_init(void)
{
    s_xl_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_ERROR(new_bus(0, BOARD_I2C1_SDA, BOARD_I2C1_SCL, &s_i2c1), TAG, "i2c1");
    ESP_RETURN_ON_ERROR(new_bus(1, BOARD_I2C2_SDA, BOARD_I2C2_SCL, &s_i2c2), TAG, "i2c2");

    i2c_device_config_t dev = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = XL9535_ADDR,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_i2c1, &dev, &s_xl9535), TAG, "xl9535 add");
    dev.device_address = BQ27220_ADDR;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_i2c1, &dev, &s_bq27220), TAG, "bq27220 add");
    ESP_RETURN_ON_ERROR(xl_read16(XL9535_REG_OUT0, &s_out), TAG, "xl9535 not responding");
    ESP_RETURN_ON_ERROR(xl_read16(XL9535_REG_CFG0, &s_cfg), TAG, "xl9535 cfg");

    // Same safe-state sequence as LilyGO's driver. POWER_EN_3V3 is held low
    // on V1.0 hardware; the vendor notes that changing it can lock the P4
    // into download mode.
    ESP_RETURN_ON_ERROR(xl_set(XIO_POWER_EN_3V3, 0), TAG, "3v3");
    ESP_RETURN_ON_ERROR(xl_set(XIO_SKY13453_VCTL, 1), TAG, "rf sw");
    ESP_RETURN_ON_ERROR(xl_set(XIO_SCREEN_RST, 0), TAG, "screen rst");
    ESP_RETURN_ON_ERROR(xl_set(XIO_TOUCH_RST, 0), TAG, "touch rst");
    ESP_RETURN_ON_ERROR(xl_set(XIO_ETH_RST, 0), TAG, "eth rst");
    ESP_RETURN_ON_ERROR(xl_set(XIO_C6_EN, 0), TAG, "c6 en");
    ESP_RETURN_ON_ERROR(xl_set(XIO_USB_PHY_PWR, 1), TAG, "usb phy");
    ESP_RETURN_ON_ERROR(xl_set(XIO_AUDIO_PWR_EN, 0), TAG, "audio pwr");
    ESP_RETURN_ON_ERROR(xl_set(XIO_SD_PWR_N, 1), TAG, "sd pwr");

    ESP_LOGI(TAG, "XL9535 ready (out=0x%04x cfg=0x%04x)", s_out, s_cfg);
    return ESP_OK;
}

esp_err_t board_audio_power(bool on)
{
    esp_err_t err = xl_set(XIO_AUDIO_PWR_EN, on);
    if (err == ESP_OK && on) vTaskDelay(pdMS_TO_TICKS(20));  // let AD_3V3 settle
    return err;
}

esp_err_t board_c6_enable(bool on)
{
    return xl_set(XIO_C6_EN, on);
}

esp_err_t board_sd_power(bool on)
{
    esp_err_t err = xl_set(XIO_SD_PWR_N, !on);
    if (err == ESP_OK && on) vTaskDelay(pdMS_TO_TICKS(10));
    return err;
}

esp_err_t board_screen_reset_release(bool released)
{
    return xl_set(XIO_SCREEN_RST, released);
}

esp_err_t board_touch_reset_release(bool released)
{
    return xl_set(XIO_TOUCH_RST, released);
}

esp_err_t board_battery_percent(uint8_t *percent)
{
    if (!percent) return ESP_ERR_INVALID_ARG;
    if (!s_bq27220) return ESP_ERR_INVALID_STATE;

    uint8_t reg = BQ27220_REG_STATE_OF_CHARGE;
    uint8_t data[2];
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(s_bq27220, &reg, 1, data, sizeof(data), 100),
                        TAG, "bq27220 state of charge");
    uint16_t value = data[0] | (data[1] << 8);
    if (value > 100) return ESP_ERR_INVALID_RESPONSE;
    *percent = value;
    return ESP_OK;
}

i2c_master_bus_handle_t board_main_i2c_bus(void)
{
    return s_i2c1;
}

i2c_master_bus_handle_t board_codec_i2c_bus(void)
{
    return s_i2c2;
}
