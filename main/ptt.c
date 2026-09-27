#include "ptt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "board.h"
#include "settings.h"
#include "zello_client.h"

// Hold-to-talk on the BOOT button, debounced by requiring a stable level for
// three consecutive 10 ms samples. With ptt_latch set, a press toggles instead,
// which is easier for a long call than holding a small button.
static void ptt_task(void *arg)
{
    bool pressed = false;
    int stable = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10));
        bool now = gpio_get_level(BOARD_BOOT_BUTTON) == 0;
        if (now == pressed) {
            stable = 0;
            continue;
        }
        if (++stable < 3) continue;
        stable = 0;
        pressed = now;

        if (g_settings.ptt_latch) {
            if (pressed) zello_set_ptt(!zello_ptt());
        } else {
            zello_set_ptt(pressed);
        }
    }
}

void ptt_button_start(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BOARD_BOOT_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    xTaskCreate(ptt_task, "ptt", 2560, NULL, 10, NULL);
}
