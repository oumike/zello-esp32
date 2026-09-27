#include "settings.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#define NS "zello"

settings_t g_settings;

typedef enum { F_STR, F_BOOL, F_U8 } ftype_t;

typedef struct {
    const char *key;
    ftype_t type;
    size_t off;
    size_t size;
    bool secret;
} field_t;

#define STR(k, secret) {#k, F_STR, offsetof(settings_t, k), sizeof(((settings_t *)0)->k), secret}
// NVS keys are limited to 15 characters; all of ours fit.
static const field_t FIELDS[] = {
    STR(username, false),
    STR(password, true),
    STR(auth_token, true),
    STR(work_network, false),
    STR(channel, false),
    STR(wifi_ssid, false),
    STR(wifi_pass, true),
    {"network", F_U8, offsetof(settings_t, network), 1, false},
    {"volume", F_U8, offsetof(settings_t, volume), 1, false},
    {"mic_gain", F_U8, offsetof(settings_t, mic_gain), 1, false},
    {"auto_connect", F_BOOL, offsetof(settings_t, auto_connect), 1, false},
    {"ptt_latch", F_BOOL, offsetof(settings_t, ptt_latch), 1, false},
};
#define NFIELDS (sizeof(FIELDS) / sizeof(FIELDS[0]))

static void set_defaults(settings_t *s)
{
    memset(s, 0, sizeof(*s));
    strlcpy(s->username, CONFIG_ZELLO_USERNAME, sizeof(s->username));
    strlcpy(s->password, CONFIG_ZELLO_PASSWORD, sizeof(s->password));
    strlcpy(s->auth_token, CONFIG_ZELLO_AUTH_TOKEN, sizeof(s->auth_token));
    strlcpy(s->work_network, CONFIG_ZELLO_WORK_NETWORK, sizeof(s->work_network));
    strlcpy(s->channel, CONFIG_ZELLO_CHANNEL, sizeof(s->channel));
    strlcpy(s->wifi_ssid, CONFIG_ZELLO_WIFI_SSID, sizeof(s->wifi_ssid));
    strlcpy(s->wifi_pass, CONFIG_ZELLO_WIFI_PASSWORD, sizeof(s->wifi_pass));
#ifdef CONFIG_ZELLO_NETWORK_WORK
    s->network = ZELLO_NET_WORK;
#else
    s->network = ZELLO_NET_CONSUMER;
#endif
    s->volume = 70;
    s->mic_gain = 30;
    s->auto_connect = true;
    s->ptt_latch = false;
}

esp_err_t settings_load(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) return err;

    set_defaults(&g_settings);
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return ESP_OK;  // first boot
    for (size_t i = 0; i < NFIELDS; i++) {
        const field_t *f = &FIELDS[i];
        void *p = (char *)&g_settings + f->off;
        if (f->type == F_STR) {
            size_t len = f->size;
            nvs_get_str(h, f->key, p, &len);
        } else {
            nvs_get_u8(h, f->key, p);
        }
    }
    nvs_close(h);
    if (g_settings.network > ZELLO_NET_WORK) g_settings.network = ZELLO_NET_CONSUMER;
    return ESP_OK;
}

esp_err_t settings_save(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    for (size_t i = 0; i < NFIELDS && err == ESP_OK; i++) {
        const field_t *f = &FIELDS[i];
        const void *p = (const char *)&g_settings + f->off;
        err = f->type == F_STR ? nvs_set_str(h, f->key, p) : nvs_set_u8(h, f->key, *(const uint8_t *)p);
    }
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t settings_set(const char *key, const char *value)
{
    // "network" takes the name as well as the number, since that is what the
    // console and the web config both have to hand.
    if (!strcmp(key, "network")) {
        if (!strcasecmp(value, "consumer") || !strcmp(value, "0")) {
            g_settings.network = ZELLO_NET_CONSUMER;
            return ESP_OK;
        }
        if (!strcasecmp(value, "work") || !strcmp(value, "1")) {
            g_settings.network = ZELLO_NET_WORK;
            return ESP_OK;
        }
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < NFIELDS; i++) {
        const field_t *f = &FIELDS[i];
        if (strcmp(f->key, key) != 0) continue;
        void *p = (char *)&g_settings + f->off;
        switch (f->type) {
        case F_STR:
            if (strlen(value) >= f->size) return ESP_ERR_INVALID_ARG;
            strlcpy(p, value, f->size);
            return ESP_OK;
        case F_BOOL:
            *(bool *)p = !strcmp(value, "1") || !strcasecmp(value, "on") ||
                         !strcasecmp(value, "true") || !strcasecmp(value, "yes");
            return ESP_OK;
        case F_U8: {
            char *end;
            long v = strtol(value, &end, 10);
            if (*end || v < 0 || v > 255) return ESP_ERR_INVALID_ARG;
            *(uint8_t *)p = v;
            return ESP_OK;
        }
        }
    }
    return ESP_ERR_NOT_FOUND;
}

void settings_print(void)
{
    for (size_t i = 0; i < NFIELDS; i++) {
        const field_t *f = &FIELDS[i];
        const void *p = (const char *)&g_settings + f->off;
        if (!strcmp(f->key, "network")) {
            printf("  %-16s %s\n", f->key, g_settings.network == ZELLO_NET_WORK ? "work" : "consumer");
            continue;
        }
        switch (f->type) {
        case F_STR:
            printf("  %-16s %s\n", f->key,
                   f->secret ? (*(const char *)p ? "********" : "(unset)") : (const char *)p);
            break;
        case F_BOOL:
            printf("  %-16s %s\n", f->key, *(const bool *)p ? "on" : "off");
            break;
        case F_U8:
            printf("  %-16s %u\n", f->key, *(const uint8_t *)p);
            break;
        }
    }
}

void settings_ws_url(char *buf, size_t size)
{
    if (g_settings.network == ZELLO_NET_WORK && g_settings.work_network[0]) {
        snprintf(buf, size, "wss://zellowork.io/ws/%s", g_settings.work_network);
    } else {
        snprintf(buf, size, "wss://zello.io/ws");
    }
}
