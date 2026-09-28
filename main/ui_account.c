// Zello account: the network (consumer or Zello Work), username, password and
// the developer token the consumer network needs. Part of onboarding after
// Wi-Fi. Afterwards the same form is the lower half of Settings, under the
// Wi-Fi network, with one Save for both.

#include <stdio.h>
#include <string.h>
#include "net_wifi.h"
#include "onboard.h"
#include "settings.h"
#include "ui_internal.h"

static lv_obj_t *s_root, *s_network, *s_user, *s_pass, *s_token, *s_work;
static lv_obj_t *s_token_label, *s_work_label;
static lv_obj_t *s_wifi_ssid, *s_wifi_pass;  // Settings only
static bool s_onboarding;

static void lower_focused(lv_event_t *e)
{
    lv_keyboard_set_mode(lv_event_get_user_data(e), LV_KEYBOARD_MODE_TEXT_LOWER);
}

// The consumer network needs the token; Zello Work needs the network name.
// Only ever show the one that applies, so neither looks optional.
static void show_relevant_fields(void)
{
    bool work = lv_dropdown_get_selected(s_network) == ZELLO_NET_WORK;
    lv_obj_t *const token_row[] = {s_token_label, s_token};
    lv_obj_t *const work_row[] = {s_work_label, s_work};
    for (int i = 0; i < 2; i++) {
        if (work) {
            lv_obj_add_flag(token_row[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(work_row[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(token_row[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(work_row[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void network_changed(lv_event_t *e)
{
    show_relevant_fields();
}

// ui_field() makes its own caption label, which the caller can't reach. These
// two fields have to be hidden caption and all, so build them by hand.
static lv_obj_t *labelled_field(lv_obj_t *col, const char *caption, const char *placeholder, lv_obj_t *kb,
                                lv_obj_t **out_label)
{
    *out_label = ui_label(col, caption, UI_FONT_SMALL, UI_COLOR_MUTED);
    lv_obj_t *ta = lv_textarea_create(col);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_obj_set_size(ta, LV_PCT(100), UI_FIELD_H);
    lv_obj_set_style_text_font(ta, UI_FONT, 0);
    lv_obj_set_style_radius(ta, UI_RADIUS, 0);
    ui_field_attach_keyboard(ta, kb);
    lv_obj_add_event_cb(ta, lower_focused, LV_EVENT_FOCUSED, kb);
    return ta;
}

static void build_fields(lv_obj_t *col, lv_obj_t *kb)
{
    ui_label(col, "Network", UI_FONT_SMALL, UI_COLOR_MUTED);
    s_network = lv_dropdown_create(col);
    lv_dropdown_set_options(s_network, "Zello (zello.io)\nZello Work");
    lv_dropdown_set_selected(s_network, g_settings.network == ZELLO_NET_WORK ? 1 : 0);
    lv_obj_set_width(s_network, LV_PCT(100));
    lv_obj_set_style_text_font(s_network, UI_FONT, 0);
    lv_obj_set_style_radius(s_network, UI_RADIUS, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(s_network), UI_FONT, 0);
    lv_obj_add_event_cb(s_network, network_changed, LV_EVENT_VALUE_CHANGED, NULL);

    s_user = ui_field(col, "Username", "Your Zello username", kb);
    lv_textarea_set_max_length(s_user, sizeof(g_settings.username) - 1);
    lv_obj_add_event_cb(s_user, lower_focused, LV_EVENT_FOCUSED, kb);

    s_pass = ui_field(col, "Password", g_settings.password[0] ? "Saved - leave blank to keep" : "Password", kb);
    lv_textarea_set_password_mode(s_pass, true);
    lv_textarea_set_max_length(s_pass, sizeof(g_settings.password) - 1);
    lv_obj_add_event_cb(s_pass, lower_focused, LV_EVENT_FOCUSED, kb);

    // A JWT is far too long to type on a touch keyboard; the web config is the
    // way in. Say so rather than letting someone start typing it here.
    s_token = labelled_field(col, "Developer token", g_settings.auth_token[0] ? "Saved" : "Paste from the web config",
                             kb, &s_token_label);
    lv_textarea_set_max_length(s_token, SETTINGS_TOKEN_MAX - 1);

    s_work = labelled_field(col, "Zello Work network", "Your network name", kb, &s_work_label);
    lv_textarea_set_max_length(s_work, sizeof(g_settings.work_network) - 1);

    lv_textarea_set_text(s_user, g_settings.username);
    lv_textarea_set_text(s_work, g_settings.work_network);
    show_relevant_fields();
}

// Validates and saves the account fields. Returns false (with a message up)
// if the form needs fixing.
static bool save_account(void)
{
    const char *err = NULL;
    const char *network = lv_dropdown_get_selected(s_network) == ZELLO_NET_WORK ? "work" : "consumer";
    esp_err_t e = onboard_save_account(network, lv_textarea_get_text(s_user), lv_textarea_get_text(s_pass),
                                       lv_textarea_get_text(s_token), lv_textarea_get_text(s_work), &err);
    if (e != ESP_OK) {
        ui_modal_message(err ? err : "Couldn't save.", UI_COLOR_WARN, true, NULL);
        return false;
    }
    return true;
}

void ui_account_destroy(void)
{
    lv_obj_t *root = s_root;
    s_root = NULL;
    s_wifi_ssid = s_wifi_pass = NULL;
    if (!s_onboarding) ui_wifi_scan_destroy();
    if (root) lv_obj_delete_async(root);  // a screen: deleted once the next one is up
}

static void close_settings(void)
{
    ui_home_show();
    ui_account_destroy();
}

// Onboarding moves on by itself once the account is saved (ui.c).
static void continue_clicked(lv_event_t *e)
{
    save_account();
}

// One Save for everything on Settings. The account applies straight away; a
// changed network is joined first and only saved (then the device restarts)
// if the join works. A failed join keeps the old network and this screen.
static void save_clicked(lv_event_t *e)
{
    const char *ssid = lv_textarea_get_text(s_wifi_ssid);
    const char *pass = lv_textarea_get_text(s_wifi_pass);
    if (!ssid[0]) {
        ui_modal_message("Enter a Wi-Fi network name,\nor tap Scan.", UI_COLOR_WARN, true, NULL);
        return;
    }
    bool same_network = !strcmp(ssid, g_settings.wifi_ssid);
    bool wifi_changed = !same_network || pass[0];

    if (!save_account()) return;
    if (!wifi_changed) {
        close_settings();
        return;
    }
    // Blank keeps the saved password on the same network; on a new one it
    // means an open network, as in setup.
    ui_wifi_join(ssid, pass[0] || !same_network ? pass : g_settings.wifi_pass, true);
}

static void cancel_clicked(lv_event_t *e)
{
    close_settings();
}

// The saved Wi-Fi password is kept when the field is left blank, but only
// while the network is still the saved one.
static void wifi_ssid_changed(lv_event_t *e)
{
    bool same = !strcmp(lv_textarea_get_text(s_wifi_ssid), g_settings.wifi_ssid);
    lv_textarea_set_placeholder_text(s_wifi_pass, same && g_settings.wifi_pass[0] ? "Saved - leave blank to keep"
                                                                                   : "Blank for an open network");
}

static void wifi_picked(const net_wifi_ap_t *ap)
{
    lv_textarea_set_text(s_wifi_ssid, ap->ssid);  // fires wifi_ssid_changed
    ui_wifi_prompt_password(s_wifi_pass, ap->secure);
}

static void wifi_scan_clicked(lv_event_t *e)
{
    lv_obj_add_flag(lv_event_get_user_data(e), LV_OBJ_FLAG_HIDDEN);  // the keyboard
    ui_wifi_scan(s_root, wifi_picked);
}

static void build_wifi_fields(lv_obj_t *col, lv_obj_t *kb)
{
    char ip[16], status[96];
    if (net_wifi_sta_ip(ip, sizeof(ip))) {
        snprintf(status, sizeof(status), LV_SYMBOL_WIFI "  Connected to \"%s\" (%s)", g_settings.wifi_ssid, ip);
    } else {
        snprintf(status, sizeof(status), LV_SYMBOL_WIFI "  Not connected");
    }
    ui_label(col, status, UI_FONT_SMALL, UI_COLOR_MUTED);
    ui_button(col, LV_SYMBOL_WIFI "  Scan for networks", UI_COLOR_ACCENT, wifi_scan_clicked, kb);

    s_wifi_ssid = ui_field(col, "Network name (SSID)", "e.g. HomeWiFi", kb);
    lv_textarea_set_max_length(s_wifi_ssid, sizeof(g_settings.wifi_ssid) - 1);
    // Plain text on purpose, as in setup: easier to get right on a touch keyboard.
    s_wifi_pass = ui_field(col, "Wi-Fi password", "", kb);
    lv_textarea_set_max_length(s_wifi_pass, sizeof(g_settings.wifi_pass) - 1);
    lv_obj_add_event_cb(s_wifi_ssid, wifi_ssid_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_textarea_set_text(s_wifi_ssid, g_settings.wifi_ssid);
}

void ui_account_onboard(void)
{
    lv_obj_t *kb;
    lv_obj_t *col = ui_screen_create(&s_root, &kb);
    s_onboarding = true;

    ui_title(col, "Zello Account");
    ui_label(col,
             "Sign in with your Zello username and password. The public zello.io network also needs a developer "
             "token from developers.zello.com - paste it in from the web config.",
             UI_FONT_SMALL, UI_COLOR_MUTED);
    build_fields(col, kb);
    ui_spacer(col);
    ui_button(col, "Continue", UI_COLOR_GO, continue_clicked, NULL);
    ui_web_hint(col);
    lv_screen_load(s_root);
}

// A screen rather than an overlay, so the Wi-Fi scan list can come and go on
// top of it.
void ui_account_edit(void)
{
    if (s_root) return;
    lv_obj_t *kb;
    lv_obj_t *col = ui_screen_create(&s_root, &kb);
    s_onboarding = false;

    ui_title(col, "Settings");
    ui_label(col, "Wi-Fi", UI_FONT, UI_COLOR_ACCENT);
    build_wifi_fields(col, kb);
    ui_label(col, "Zello account", UI_FONT, UI_COLOR_ACCENT);
    build_fields(col, kb);
    ui_label(col, "The account applies right away. A new Wi-Fi network is tried first, then the device restarts on it.",
             UI_FONT_SMALL, UI_COLOR_MUTED);
    ui_spacer(col);
    ui_button(col, LV_SYMBOL_SAVE "  Save", UI_COLOR_GO, save_clicked, NULL);
    ui_button(col, "Cancel", UI_COLOR_PANEL, cancel_clicked, NULL);
    lv_screen_load(s_root);
}
