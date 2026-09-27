#pragma once

// The Opus encoder and decoder pair used for Zello streams, plus the codec
// header Zello wants in start_stream.
//
// Transmit is fixed at AUDIO_RATE mono in AUDIO_FRAME_MS packets - that is
// what we advertise. Receive is not: the decoder is created at AUDIO_RATE and
// libopus resamples whatever the sender used, so a phone streaming at 8 or
// 24 kHz plays back correctly with nothing extra on our side.

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t opus_codec_init(void);

// Encodes one AUDIO_FRAME-sample mono frame. Returns the packet length, or a
// negative value on failure.
int opus_codec_encode(const int16_t *pcm, uint8_t *out, size_t out_size);

// Decodes one packet into `pcm`. Returns the sample count, or negative.
int opus_codec_decode(const uint8_t *packet, size_t len, int16_t *pcm, size_t max_samples);

// Forgets the decoder's inter-packet state. Call between incoming streams so
// one talker's tail doesn't bleed into the next.
void opus_codec_reset_decoder(void);

// Zello's 4-byte Opus codec header for our encoder, base64-encoded:
// sample rate (little-endian uint16), frames per packet, frame length in ms.
const char *opus_codec_header_base64(void);
