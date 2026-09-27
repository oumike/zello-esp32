#include "xml_util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

esp_err_t xml_write_text(xml_write_cb_t write, void *ctx, const char *text)
{
    return write(ctx, text, strlen(text));
}

esp_err_t xml_write_escaped(xml_write_cb_t write, void *ctx, const char *text)
{
    const char *start = text;
    for (const char *p = text;; p++) {
        const char *entity = NULL;
        switch (*p) {
        case '&': entity = "&amp;"; break;
        case '<': entity = "&lt;"; break;
        case '>': entity = "&gt;"; break;
        case '"': entity = "&quot;"; break;
        case '\'': entity = "&apos;"; break;
        default: break;
        }
        if (entity) {
            if (p > start && write(ctx, start, p - start) != ESP_OK) return ESP_FAIL;
            if (xml_write_text(write, ctx, entity) != ESP_OK) return ESP_FAIL;
            start = p + 1;
        }
        if (!*p) return p > start ? write(ctx, start, p - start) : ESP_OK;
    }
}

esp_err_t xml_write_attr(xml_write_cb_t write, void *ctx, const char *name, const char *value)
{
    if (xml_write_text(write, ctx, " ") != ESP_OK || xml_write_text(write, ctx, name) != ESP_OK ||
        xml_write_text(write, ctx, "=\"") != ESP_OK || xml_write_escaped(write, ctx, value) != ESP_OK) {
        return ESP_FAIL;
    }
    return xml_write_text(write, ctx, "\"");
}

esp_err_t xml_write_attr_u32(xml_write_cb_t write, void *ctx, const char *name, uint32_t value)
{
    char text[12];
    snprintf(text, sizeof(text), "%lu", (unsigned long)value);
    return xml_write_attr(write, ctx, name, text);
}

esp_err_t xml_write_attr_bool(xml_write_cb_t write, void *ctx, const char *name, bool value)
{
    return xml_write_attr(write, ctx, name, value ? "true" : "false");
}

int xml_attr(const char *element, const char *end, const char *name, char *out, size_t size)
{
    size_t name_len = strlen(name);
    // Start one past '<' so p[-1] is always readable: an attribute name has to
    // follow whitespace, which keeps "name" from matching "nickname".
    for (const char *p = element + 1; p + name_len + 2 < end; p++) {
        if (!isspace((unsigned char)p[-1]) || strncmp(p, name, name_len) || p[name_len] != '=') continue;
        char quote = p[name_len + 1];
        if (quote != '"' && quote != '\'') return -1;
        const char *value = p + name_len + 2;
        size_t used = 0;
        while (value < end && *value != quote) {
            char decoded;
            if (*value != '&') {
                decoded = *value++;
            } else if (!strncmp(value, "&amp;", 5)) {
                decoded = '&'; value += 5;
            } else if (!strncmp(value, "&lt;", 4)) {
                decoded = '<'; value += 4;
            } else if (!strncmp(value, "&gt;", 4)) {
                decoded = '>'; value += 4;
            } else if (!strncmp(value, "&quot;", 6)) {
                decoded = '"'; value += 6;
            } else if (!strncmp(value, "&apos;", 6)) {
                decoded = '\''; value += 6;
            } else {
                return -1;
            }
            if (used + 1 >= size) return -1;
            out[used++] = decoded;
        }
        if (value >= end) return -1;
        out[used] = 0;
        return 1;
    }
    return 0;
}

bool xml_attr_bool(const char *element, const char *end, const char *name, bool *value)
{
    char text[8];
    if (xml_attr(element, end, name, text, sizeof(text)) != 1) return false;
    if (!strcmp(text, "true")) *value = true;
    else if (!strcmp(text, "false")) *value = false;
    else return false;
    return true;
}

bool xml_attr_u32(const char *element, const char *end, const char *name, uint32_t max, uint32_t *value)
{
    char text[12];
    if (xml_attr(element, end, name, text, sizeof(text)) != 1) return false;
    char *tail;
    unsigned long parsed = strtoul(text, &tail, 10);
    if (!text[0] || *tail || parsed > max) return false;
    *value = parsed;
    return true;
}

const char *xml_find_element(const char *from, const char *name)
{
    size_t len = strlen(name);
    for (const char *p = strchr(from, '<'); p; p = strchr(p + 1, '<')) {
        if (strncmp(p + 1, name, len)) continue;
        char next = p[1 + len];
        if (isspace((unsigned char)next) || next == '>' || next == '/') return p;
    }
    return NULL;
}
