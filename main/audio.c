#include "audio.h"

#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "board.h"

static const char *TAG = "audio";

// 2 s of receive buffering; start playback once two Zello packets (120 ms)
// have arrived.
#define PLAY_BUF_BYTES    (2 * AUDIO_RATE * sizeof(int16_t))
#define PLAY_START_BYTES  (2 * AUDIO_FRAME * sizeof(int16_t))

static esp_codec_dev_handle_t s_codec;
static StreamBufferHandle_t s_play;
static audio_mic_cb_t s_mic_cb;
static volatile bool s_playing;
static volatile bool s_loopback;
static volatile uint8_t s_mic_level, s_play_level;

static esp_err_t codec_open(void)
{
    i2s_chan_handle_t tx, rx;
    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan.auto_clear = true;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan, &tx, &rx), TAG, "i2s channel");

    i2s_std_config_t std = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = BOARD_I2S_MCLK,
            .bclk = BOARD_I2S_BCLK,
            .ws = BOARD_I2S_WS,
            .dout = BOARD_I2S_DOUT,
            .din = BOARD_I2S_DIN,
        },
    };
    std.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(tx, &std), TAG, "i2s tx");
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(rx, &std), TAG, "i2s rx");

    audio_codec_i2s_cfg_t i2s_cfg = {.port = I2S_NUM_0, .rx_handle = rx, .tx_handle = tx};
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = 1,
        .addr = BOARD_ES8311_ADDR << 1,  // esp_codec_dev takes the 8-bit address
        .bus_handle = board_codec_i2c_bus(),
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    ESP_RETURN_ON_FALSE(data_if && ctrl_if && gpio_if, ESP_FAIL, TAG, "codec interfaces");

    es8311_codec_cfg_t es_cfg = {
        .ctrl_if = ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .pa_pin = -1,  // NS4150B is powered with the audio rail; no separate enable
        .use_mclk = true,
        .digital_mic = false,  // electret on MIC1P/MIC1N
    };
    const audio_codec_if_t *codec_if = es8311_codec_new(&es_cfg);
    ESP_RETURN_ON_FALSE(codec_if, ESP_FAIL, TAG, "es8311");

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
        .codec_if = codec_if,
        .data_if = data_if,
    };
    s_codec = esp_codec_dev_new(&dev_cfg);
    ESP_RETURN_ON_FALSE(s_codec, ESP_FAIL, TAG, "codec dev");

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 1,
        .sample_rate = AUDIO_RATE,
        .mclk_multiple = 256,
    };
    ESP_RETURN_ON_FALSE(esp_codec_dev_open(s_codec, &fs) == ESP_CODEC_DEV_OK, ESP_FAIL, TAG, "codec open");
    return ESP_OK;
}

static uint8_t peak_percent(const int16_t *pcm, size_t samples)
{
    int32_t peak = 0;
    for (size_t i = 0; i < samples; i++) {
        int32_t v = pcm[i] < 0 ? -(int32_t)pcm[i] : pcm[i];
        if (v > peak) peak = v;
    }
    return (uint8_t)(peak * 100 / 32768);
}

static void play_task(void *arg)
{
    int16_t *frame = malloc(AUDIO_FRAME * sizeof(int16_t));
    if (!frame) vTaskDelete(NULL);
    for (;;) {
        size_t got = 0;
        if (!s_playing && xStreamBufferBytesAvailable(s_play) >= PLAY_START_BYTES) {
            s_playing = true;
        }
        if (s_playing) {
            got = xStreamBufferReceive(s_play, frame, AUDIO_FRAME * sizeof(int16_t), 0);
            if (got < AUDIO_FRAME * sizeof(int16_t)) s_playing = false;  // drained: re-prebuffer
        }
        // Short read (or nothing buffered): pad the rest of the frame with
        // silence so the codec keeps its cadence. The clamp is for the
        // compiler's benefit; the stream buffer never returns more than asked.
        if (got > sizeof(int16_t) * AUDIO_FRAME) got = sizeof(int16_t) * AUDIO_FRAME;
        memset((uint8_t *)frame + got, 0, AUDIO_FRAME * sizeof(int16_t) - got);
        s_play_level = got ? peak_percent(frame, got / sizeof(int16_t)) : 0;
        // Blocks for ~60 ms; the I2S clock paces this loop.
        esp_codec_dev_write(s_codec, frame, AUDIO_FRAME * sizeof(int16_t));
    }
}

static void mic_task(void *arg)
{
    int16_t *frame = malloc(AUDIO_FRAME * sizeof(int16_t));
    if (!frame) vTaskDelete(NULL);
    for (;;) {
        if (esp_codec_dev_read(s_codec, frame, AUDIO_FRAME * sizeof(int16_t)) != ESP_CODEC_DEV_OK) {
            vTaskDelay(pdMS_TO_TICKS(AUDIO_FRAME_MS));
            continue;
        }
        s_mic_level = peak_percent(frame, AUDIO_FRAME);
        if (s_loopback) {
            xStreamBufferSend(s_play, frame, AUDIO_FRAME * sizeof(int16_t), 0);
        } else if (s_mic_cb) {
            s_mic_cb(frame, AUDIO_FRAME);
        }
    }
}

esp_err_t audio_init(audio_mic_cb_t mic_cb)
{
    s_mic_cb = mic_cb;
    s_play = xStreamBufferCreate(PLAY_BUF_BYTES, 1);
    ESP_RETURN_ON_FALSE(s_play, ESP_ERR_NO_MEM, TAG, "play buffer");

    ESP_RETURN_ON_ERROR(board_audio_power(true), TAG, "audio power");
    ESP_RETURN_ON_ERROR(codec_open(), TAG, "codec");

    xTaskCreate(play_task, "audio_play", 4096, NULL, 20, NULL);
    xTaskCreate(mic_task, "audio_mic", 4096, NULL, 19, NULL);
    ESP_LOGI(TAG, "ES8311 running at %d Hz mono, %d ms frames", AUDIO_RATE, AUDIO_FRAME_MS);
    return ESP_OK;
}

void audio_play_push(const int16_t *pcm, size_t samples)
{
    // Drop rather than block the decode task if we're somehow way behind.
    xStreamBufferSend(s_play, pcm, samples * sizeof(int16_t), 0);
}

void audio_play_flush(void)
{
    xStreamBufferReset(s_play);
}

bool audio_is_playing(void)
{
    return s_playing;
}

void audio_set_volume(int percent)
{
    if (s_codec) esp_codec_dev_set_out_vol(s_codec, percent);
}

void audio_set_mic_gain(int db)
{
    if (s_codec) esp_codec_dev_set_in_gain(s_codec, db);
}

void audio_set_loopback(bool on)
{
    s_loopback = on;
    if (!on) audio_play_flush();
}

bool audio_loopback(void)
{
    return s_loopback;
}

uint8_t audio_mic_level(void)
{
    return s_mic_level;
}

uint8_t audio_play_level(void)
{
    return s_play_level;
}
