/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ARM NEON optimized frame difference detection
 *
 * Compares two RGB24 frames and counts differing pixels
 */

#pragma once

#if defined(__aarch64__) || defined(__ARM_NEON)

#include <arm_neon.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Fast frame difference using NEON SIMD
 *
 * Returns the number of differing pixels between two frames
 * Processes 16 pixels (48 bytes) per iteration
 */
static inline size_t framediff_count_neon(const uint8_t *restrict frame1,
                                          const uint8_t *restrict frame2,
                                          size_t pixel_count)
{
    const size_t bytes_per_pixel = 3;
    const size_t total_bytes = pixel_count * bytes_per_pixel;
    size_t diff_count = 0;
    size_t i = 0;

    /* Process 48 bytes (16 pixels) per iteration using SIMD */
    for (; i + 48 <= total_bytes; i += 48) {
        /* Load 48 bytes from each frame */
        uint8x16x3_t v1 = vld3q_u8(frame1 + i);
        uint8x16x3_t v2 = vld3q_u8(frame2 + i);

        /* Compare each channel (R, G, B) */
        uint8x16_t cmp_r = vceqq_u8(v1.val[0], v2.val[0]);
        uint8x16_t cmp_g = vceqq_u8(v1.val[1], v2.val[1]);
        uint8x16_t cmp_b = vceqq_u8(v1.val[2], v2.val[2]);

        /* Combine: pixel is same only if all channels match */
        uint8x16_t same_pixels = vandq_u8(vandq_u8(cmp_r, cmp_g), cmp_b);

        /* Count different pixels (invert the mask)
         * Each different pixel produces 0xFF, same pixels produce 0x00 */
        uint8x16_t diff_pixels = vmvnq_u8(same_pixels);

        /* Shift right to convert 0xFF to 0x01 (or any non-zero to 0)
         * This creates a mask where different pixels = 1 */
        uint8x16_t diff_mask = vshrq_n_u8(diff_pixels, 7);

        /* Horizontal add: sum all 16 bytes using pairwise addition */
        uint16x8_t sum16 = vpaddlq_u8(diff_mask);
        uint32x4_t sum32 = vpaddlq_u16(sum16);
        uint64x2_t sum64 = vpaddlq_u32(sum32);

        /* Extract final count */
        diff_count += vgetq_lane_u64(sum64, 0) + vgetq_lane_u64(sum64, 1);
    }

    /* Fallback to scalar for remainder */
    for (; i < total_bytes; i += 3) {
        if (frame1[i] != frame2[i] || frame1[i + 1] != frame2[i + 1] ||
            frame1[i + 2] != frame2[i + 2]) {
            diff_count++;
        }
    }

    return diff_count;
}

/* Calculate difference percentage (0-100) */
static inline int framediff_percentage_neon(const uint8_t *restrict frame1,
                                            const uint8_t *restrict frame2,
                                            size_t pixel_count)
{
    size_t diff_pixels = framediff_count_neon(frame1, frame2, pixel_count);
    return (int) ((diff_pixels * 100) / pixel_count);
}

#endif /* __aarch64__ || __ARM_NEON */
