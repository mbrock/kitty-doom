/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Base64 encoding with SIMD optimization
 *
 * Provides portable scalar implementation and unified API with runtime
 * CPU feature detection to select the fastest available implementation.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/* Portable scalar base64 encoding
 *
 * This is the fallback implementation for platforms without SIMD support,
 * or for handling remainder bytes in SIMD implementations.
 */

static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Basic scalar base64 encoding
 *
 * Processes 3 input bytes to 4 output base64 characters at a time.
 * Returns the number of output bytes written.
 */
static inline size_t base64_encode_scalar(const uint8_t *restrict input,
                                          size_t input_len,
                                          uint8_t *restrict output)
{
    size_t output_len = 0;
    size_t i;

    /* Process complete 3-byte groups */
    for (i = 0; i + 2 < input_len; i += 3) {
        uint32_t triple = ((uint32_t) input[i] << 16) |
                          ((uint32_t) input[i + 1] << 8) |
                          ((uint32_t) input[i + 2]);

        output[output_len++] = base64_table[(triple >> 18) & 0x3F];
        output[output_len++] = base64_table[(triple >> 12) & 0x3F];
        output[output_len++] = base64_table[(triple >> 6) & 0x3F];
        output[output_len++] = base64_table[triple & 0x3F];
    }

    /* Handle remainder (0, 1, or 2 bytes) */
    size_t remaining = input_len - i;

    if (remaining == 2) {
        /* 2 bytes remaining: output 3 base64 chars + 1 padding */
        uint32_t triple =
            ((uint32_t) input[i] << 16) | ((uint32_t) input[i + 1] << 8);

        output[output_len++] = base64_table[(triple >> 18) & 0x3F];
        output[output_len++] = base64_table[(triple >> 12) & 0x3F];
        output[output_len++] = base64_table[(triple >> 6) & 0x3F];
        output[output_len++] = '=';
    } else if (remaining == 1) {
        /* 1 byte remaining: output 2 base64 chars + 2 padding */
        uint32_t triple = (uint32_t) input[i] << 16;

        output[output_len++] = base64_table[(triple >> 18) & 0x3F];
        output[output_len++] = base64_table[(triple >> 12) & 0x3F];
        output[output_len++] = '=';
        output[output_len++] = '=';
    }

    return output_len;
}

/* Scalar unrolled version removed - NEON provides sufficient speedup
 * and fallback to basic scalar is acceptable for non-ARM platforms
 */

/* Base64 encoding function pointer type */
typedef size_t (*base64_encode_func_t)(const uint8_t *restrict input,
                                       size_t input_len,
                                       uint8_t *restrict output);

/* Unified API: automatically selects best implementation */
size_t base64_encode_auto(const uint8_t *restrict input,
                          size_t input_len,
                          uint8_t *restrict output);

/* Get the name of the active implementation (for debugging) */
const char *base64_get_impl_name(void);
