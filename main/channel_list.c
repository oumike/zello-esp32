#include "channel_list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"

static const char *TAG = "channels";

#define DATA_MOUNT "/data"
#define DATA_LABEL "storage"
#define TMP_PATH   DATA_MOUNT "/zello-channels.tmp"

static channel_entry_t s_list[CHANNEL_LIST_MAX];
static size_t s_count;
static SemaphoreHandle_t s_lock;
static wl_handle_t s_wl = WL_INVALID_HANDLE;
static void (*s_changed_cb)(void);

static void lock(void) { xSemaphoreTake(s_lock, portMAX_DELAY); }
static void unlock(void) { xSemaphoreGive(s_lock); }

// ---- ordering --------------------------------------------------------------

static int compare(const void *a, const void *b)
{
    const channel_entry_t *x = a, *y = b;
    if (x->favorite != y->favorite) return x->favorite ? -1 : 1;
    if (x->uses != y->uses) return x->uses > y->uses ? -1 : 1;
    return strcasecmp(x->name, y->name);
}

static void sort_list(void)
{
    qsort(s_list, s_count, sizeof(s_list[0]), compare);
}

// Index of `name` in the list, or -1.
static int index_of(const char *name)
{
    for (size_t i = 0; i < s_count; i++) {
        if (!strcasecmp(s_list[i].name, name)) return (int)i;
    }
    return -1;
}

// ---- storage ---------------------------------------------------------------

static esp_err_t file_write(void *ctx, const char *data, size_t len)
{
    return fwrite(data, 1, len, ctx) == len ? ESP_OK : ESP_FAIL;
}

esp_err_t channel_list_write(xml_write_cb_t write, void *ctx, const char *indent)
{
    esp_err_t err = ESP_OK;
    for (size_t i = 0; i < s_count && err == ESP_OK; i++) {
        const channel_entry_t *e = &s_list[i];
        err = xml_write_text(write, ctx, indent);
        if (err == ESP_OK) err = xml_write_text(write, ctx, "<channel");
        if (err == ESP_OK) err = xml_write_attr(write, ctx, "name", e->name);
        if (err == ESP_OK) err = xml_write_attr(write, ctx, "desc", e->desc);
        if (err == ESP_OK) err = xml_write_attr_bool(write, ctx, "favorite", e->favorite);
        if (err == ESP_OK) err = xml_write_attr_u32(write, ctx, "uses", e->uses);
        if (err == ESP_OK) err = xml_write_text(write, ctx, "/>\n");
    }
    return err;
}

// Writes to a temporary file and renames it over the real one, so a reset
// halfway through a save cannot leave a half-written list behind.
static esp_err_t save_locked(void)
{
    if (s_wl == WL_INVALID_HANDLE) return ESP_ERR_INVALID_STATE;
    FILE *f = fopen(TMP_PATH, "w");
    if (!f) return ESP_FAIL;
    esp_err_t err = xml_write_text(file_write, f,
                                   "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                   "<zello-channels version=\"1\">\n");
    if (err == ESP_OK) err = channel_list_write(file_write, f, "  ");
    if (err == ESP_OK) err = xml_write_text(file_write, f, "</zello-channels>\n");
    if (fclose(f) != 0) err = ESP_FAIL;
    if (err != ESP_OK) {
        remove(TMP_PATH);
        return err;
    }
    remove(CHANNEL_LIST_PATH);
    return rename(TMP_PATH, CHANNEL_LIST_PATH) == 0 ? ESP_OK : ESP_FAIL;
}

// Fills the list from `xml`. Returns the number of entries parsed.
static size_t parse_locked(const char *xml)
{
    s_count = 0;
    for (const char *p = xml_find_element(xml, "channel"); p && s_count < CHANNEL_LIST_MAX;
         p = xml_find_element(p + 1, "channel")) {
        const char *end = strchr(p, '>');
        if (!end) break;
        channel_entry_t e = {0};
        if (xml_attr(p, end, "name", e.name, sizeof(e.name)) != 1 || !e.name[0]) continue;
        xml_attr(p, end, "desc", e.desc, sizeof(e.desc));
        xml_attr_bool(p, end, "favorite", &e.favorite);
        xml_attr_u32(p, end, "uses", UINT32_MAX, &e.uses);
        if (index_of(e.name) >= 0) continue;  // keep the first of a duplicated name
        s_list[s_count++] = e;
    }
    sort_list();
    return s_count;
}

static void notify(void)
{
    if (s_changed_cb) s_changed_cb();
}

esp_err_t channel_list_init(void)
{
    if (!s_lock) s_lock = xSemaphoreCreateMutex();

    esp_vfs_fat_mount_config_t cfg = {
        .format_if_mount_failed = true,
        .max_files = 4,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(DATA_MOUNT, DATA_LABEL, &cfg, &s_wl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "could not mount %s: %s", DATA_MOUNT, esp_err_to_name(err));
        return err;
    }

    FILE *f = fopen(CHANNEL_LIST_PATH, "r");
    if (!f) {
        ESP_LOGI(TAG, "no channel list yet");
        return ESP_OK;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    // A full list of 128 channels is well under 32 KB; anything larger is not
    // a list we wrote.
    if (size <= 0 || size > 64 * 1024) {
        fclose(f);
        ESP_LOGW(TAG, "ignoring a %ld byte channel list", size);
        return ESP_OK;
    }
    char *xml = malloc(size + 1);
    if (!xml) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    size_t got = fread(xml, 1, size, f);
    bool ok = !ferror(f);
    fclose(f);
    xml[ok ? got : 0] = 0;

    lock();
    size_t n = parse_locked(xml);
    unlock();
    free(xml);
    ESP_LOGI(TAG, "loaded %u channels", (unsigned)n);
    return ESP_OK;
}

// ---- reads -----------------------------------------------------------------

size_t channel_list_count(void)
{
    lock();
    size_t n = s_count;
    unlock();
    return n;
}

size_t channel_list_favorite_count(void)
{
    lock();
    size_t n = 0;
    for (size_t i = 0; i < s_count; i++) {
        if (s_list[i].favorite) n++;
    }
    unlock();
    return n;
}

static bool matches(const channel_entry_t *e, const char *query)
{
    return !query || !query[0] || strcasestr(e->name, query) || strcasestr(e->desc, query);
}

size_t channel_list_get(const char *query, bool favorites_only, channel_entry_t *out, size_t max)
{
    lock();
    size_t n = 0;
    for (size_t i = 0; i < s_count && n < max; i++) {
        if (favorites_only && !s_list[i].favorite) continue;
        if (!matches(&s_list[i], query)) continue;
        out[n++] = s_list[i];
    }
    unlock();
    return n;
}

bool channel_list_find(const char *name, channel_entry_t *out)
{
    lock();
    int i = index_of(name);
    if (i >= 0 && out) *out = s_list[i];
    unlock();
    return i >= 0;
}

// ---- writes ----------------------------------------------------------------

esp_err_t channel_list_put(const char *orig_name, const channel_entry_t *entry)
{
    if (!entry->name[0]) return ESP_ERR_INVALID_ARG;
    lock();
    int at = orig_name && orig_name[0] ? index_of(orig_name) : -1;
    // A rename onto an existing name replaces that entry rather than making a
    // second one with the same name.
    int clash = index_of(entry->name);
    if (clash >= 0 && clash != at) {
        if (at >= 0) {
            s_list[at] = s_list[s_count - 1];
            s_count--;
            if (clash == (int)s_count) clash = at;
        }
        at = clash;
    }
    if (at < 0) {
        if (s_count >= CHANNEL_LIST_MAX) {
            unlock();
            return ESP_ERR_NO_MEM;
        }
        at = (int)s_count++;
    }
    s_list[at] = *entry;
    sort_list();
    esp_err_t err = save_locked();
    unlock();
    if (err == ESP_OK) notify();
    return err;
}

esp_err_t channel_list_delete(const char *name)
{
    lock();
    int at = index_of(name);
    if (at < 0) {
        unlock();
        return ESP_ERR_NOT_FOUND;
    }
    s_list[at] = s_list[--s_count];
    sort_list();
    esp_err_t err = save_locked();
    unlock();
    if (err == ESP_OK) notify();
    return err;
}

esp_err_t channel_list_set_favorite(const char *name, bool favorite)
{
    lock();
    int at = index_of(name);
    if (at < 0) {
        unlock();
        return ESP_ERR_NOT_FOUND;
    }
    s_list[at].favorite = favorite;
    sort_list();
    esp_err_t err = save_locked();
    unlock();
    if (err == ESP_OK) notify();
    return err;
}

void channel_list_record_join(const char *name)
{
    if (!name || !name[0]) return;
    lock();
    int at = index_of(name);
    if (at < 0) {
        if (s_count >= CHANNEL_LIST_MAX) {
            unlock();
            return;
        }
        at = (int)s_count++;
        s_list[at] = (channel_entry_t){0};
        strlcpy(s_list[at].name, name, sizeof(s_list[at].name));
    }
    s_list[at].uses++;
    sort_list();
    esp_err_t err = save_locked();
    unlock();
    if (err != ESP_OK) ESP_LOGW(TAG, "could not record the join of \"%s\": %s", name, esp_err_to_name(err));
    notify();
}

esp_err_t channel_list_import_buffer(const char *xml, size_t *count)
{
    channel_entry_t *saved = malloc(sizeof(s_list));
    if (!saved) return ESP_ERR_NO_MEM;

    lock();
    memcpy(saved, s_list, sizeof(s_list));
    size_t saved_count = s_count;
    size_t n = parse_locked(xml);
    esp_err_t err = n ? save_locked() : ESP_ERR_NOT_FOUND;
    if (err != ESP_OK) {  // leave the list as it was
        memcpy(s_list, saved, sizeof(s_list));
        s_count = saved_count;
    }
    unlock();
    free(saved);
    if (count) *count = n;
    if (err == ESP_OK) notify();
    return err;
}

esp_err_t channel_list_replace(const channel_entry_t *entries, size_t n, size_t *count)
{
    channel_entry_t *saved = malloc(sizeof(s_list));
    if (!saved) return ESP_ERR_NO_MEM;

    lock();
    memcpy(saved, s_list, sizeof(s_list));
    size_t saved_count = s_count;
    s_count = 0;
    for (size_t i = 0; i < n && s_count < CHANNEL_LIST_MAX; i++) {
        if (!entries[i].name[0] || index_of(entries[i].name) >= 0) continue;  // keep the first of a name
        s_list[s_count++] = entries[i];
    }
    sort_list();
    size_t added = s_count;
    esp_err_t err = added ? save_locked() : ESP_ERR_NOT_FOUND;
    if (err != ESP_OK) {  // leave the list as it was
        memcpy(s_list, saved, sizeof(s_list));
        s_count = saved_count;
    }
    unlock();
    free(saved);
    if (count) *count = added;
    if (err == ESP_OK) notify();
    return err;
}

void channel_list_set_changed_cb(void (*cb)(void))
{
    s_changed_cb = cb;
}
