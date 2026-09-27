#include "sdcard.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/sdmmc_host.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "sdmmc_cmd.h"
#include "board.h"

static const char *TAG = "sdcard";

// Slot 0 uses the IO MUX pins 39-44 (CLK 43, CMD 44, D0-D3 39-42), whose IO
// supply is the P4's on-chip LDO channel 4, as on Espressif's P4 boards and
// in the Arduino SD_MMC setup camillia-mt uses on this board.
#define SD_LDO_CHAN 4

static sdmmc_card_t *s_card;
static sd_pwr_ctrl_handle_t s_pwr;
static SemaphoreHandle_t s_lock;

bool sdcard_mounted(void)
{
    return s_card != NULL;
}

esp_err_t sdcard_mount(void)
{
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_card) {
        // Still there? A pulled card fails this status query.
        if (sdmmc_get_status(s_card) == ESP_OK) {
            xSemaphoreGive(s_lock);
            return ESP_OK;
        }
        ESP_LOGW(TAG, "card gone, remounting");
        esp_vfs_fat_sdcard_unmount(SDCARD_MOUNT, s_card);
        s_card = NULL;
    }

    esp_err_t err = board_sd_power(true);
    if (err == ESP_OK && !s_pwr) {
        sd_pwr_ctrl_ldo_config_t ldo = {.ldo_chan_id = SD_LDO_CHAN};
        err = sd_pwr_ctrl_new_on_chip_ldo(&ldo, &s_pwr);
    }
    if (err == ESP_OK) {
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.slot = SDMMC_HOST_SLOT_0;
        host.pwr_ctrl_handle = s_pwr;
        sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
        slot.width = 4;
        esp_vfs_fat_sdmmc_mount_config_t mcfg = {
            .format_if_mount_failed = false,
            .max_files = 4,
            .allocation_unit_size = 16 * 1024,
        };
        err = esp_vfs_fat_sdmmc_mount(SDCARD_MOUNT, &host, &slot, &mcfg, &s_card);
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "mounted %s, %llu MB", s_card->cid.name,
                 (unsigned long long)s_card->csd.capacity * s_card->csd.sector_size / (1024 * 1024));
    } else {
        ESP_LOGW(TAG, "no card: %s", esp_err_to_name(err));
        s_card = NULL;
        board_sd_power(false);
    }
    xSemaphoreGive(s_lock);
    return err;
}
