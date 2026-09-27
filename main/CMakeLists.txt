idf_component_register(
    SRCS "app_main.c" "board.c" "display.c" "audio.c" "ptt.c" "sdcard.c"
         "lv_mem_psram.c" "net_wifi.c" "settings.c" "xml_util.c" "channel_list.c"
         "opus_codec.c" "zello_client.c" "onboard.c" "web_config.c"
         "console_cmds.c"
         "ui.c" "ui_wifi.c" "ui_account.c" "ui_channels.c" "ui_home.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES "web/index.html"
    PRIV_REQUIRES opus esp_driver_i2c esp_driver_i2s esp_driver_gpio
                  esp_driver_sdmmc esp_netif esp_event esp_wifi nvs_flash console
                  lwip esp_timer esp_lcd esp_lvgl_port lvgl fatfs wear_levelling
                  esp_http_server esp_websocket_client esp-tls mbedtls json sdmmc)
