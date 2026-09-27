// Zello account: the network (consumer or Zello Work), username, password and
// the developer token the consumer network needs. Part of onboarding after
// Wi-Fi, and editable later from Home.

#include <string.h>
#include "onboard.h"
#include "settings.h"
#include "ui_internal.h"

static lv_obj_t *s_root, *s_network, *s_user, *s_pass, *s_token, *s_work;
static lv_obj_t *s_token_label, *s_work_label;
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

// Validates and applies the form. Returns false (with a message up) if the
// form needs fixing.
static bool apply(void)
{
    const char *err = NULL;
    bool onboarding = s_onboarding;
    const char *network = lv_dropdown_get_selected(s_network) == ZELLO_NET_WORK ? "work" : "consumer";
    esp_err_t e = onboard_save_account(network, lv_textarea_get_text(s_user), lv_textarea_get_text(s_pass),
                                       lv_textarea_get_text(s_token), lv_textarea_get_text(s_work), &err);
    if (e != ESP_OK) {
        ui_modal_message(err ? err : "Couldn't save.", UI_COLOR_WARN, true, NULL);
        return false;
    }
    // Onboarding moves on by itself (ui.c); an edit just closes.
    if (!onboarding) ui_account_destroy();
    return true;
}

void ui_account_destroy(void)
{
    lv_obj_t *root = s_root;
    s_root = NULL;
    if (!root) return;
    if (s_onboarding) {
        lv_obj_delete_async(root);  // a screen: deleted once the next one is up
    } else {
        lv_obj_delete(root);  // an overlay on the top layer
    }
}

// Saving advances onboarding (ui.c) and closes this form.
static void save_clicked(lv_event_t *e)
{
    apply();
}

static void cancel_clicked(lv_event_t *e)
{
    ui_account_destroy();
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
    ui_button(col, "Continue", UI_COLOR_GO, save_clicked, NULL);
    ui_web_hint_update(ui_label(col, "", UI_FONT_SMALL, UI_COLOR_MUTED));
    lv_screen_load(s_root);
}

void ui_account_edit(void)
{
    if (s_root) return;
    lv_obj_t *kb;
    lv_obj_t *col = ui_layout_create(lv_layer_top(), &s_root, &kb);
    s_onboarding = false;

    ui_title(col, "Zello Account");
    ui_label(col, "Changes take effect right away: the device signs in again.", UI_FONT_SMALL, UI_COLOR_MUTED);
    build_fields(col, kb);
    ui_spacer(col);
    ui_button(col, LV_SYMBOL_SAVE "  Save", UI_COLOR_GO, save_clicked, NULL);
    ui_button(col, "Cancel", UI_COLOR_PANEL, cancel_clicked, NULL);
}
