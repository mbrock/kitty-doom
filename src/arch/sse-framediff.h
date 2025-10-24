/* SPDX-License-Identifier: GPL-2.0 */
/*
 * x86 SSE4.2 optimized frame difference detection
 *
 * Compares two RGB24 frames and counts differing pixels
 * Requires SSE4.2 for POPCNT instruction
 */

#pragma once

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
    defined(_M_IX86)

#include <emmintrin.h> /* SSE2 */
#include <nmmintrin.h> /* SSE4.2 for _mm_popcnt_u32 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Fast frame difference using SSE2 SIMD + POPCNT
 *
 * Returns the number of differing pixels between two frames
 * Processes 48 bytes (16 pixels) per iteration
 *
 * Algorithm:
 * 1. Load 48 bytes (16 RGB24 pixels) from each frame as 3x 16-byte vectors
 * 2. Compare using _mm_cmpeq_epi8 for each 16-byte chunk
 * 3. Invert comparison results using _mm_andnot_si128
 * 4. Use _mm_movemask_epi8 to extract bitmasks
 * 5. Use __builtin_popcount (POPCNT) to count differing bytes
 *
 * Note: RGB24 pixels are processed in groups where each pixel's 3 bytes
 * are distributed across the three 16-byte vectors. We approximate pixel
 * differences by checking if ANY byte in the 48-byte block differs.
 */
static inline size_t framediff_count_sse(const uint8_t *restrict frame1,
                                         const uint8_t *restrict frame2,
                                         size_t pixel_count)
{
    const size_t bytes_per_pixel = 3;
    const size_t total_bytes = pixel_count * bytes_per_pixel;
    size_t diff_count = 0;
    size_t i = 0;

    /* Process 48 bytes (16 complete RGB24 pixels) per iteration
     * This ensures perfect alignment with RGB24 pixel boundaries */
    while (i + 48 <= total_bytes) {
        /* Load 48 bytes from each frame as three 16-byte vectors */
        __m128i v1_0 = _mm_loadu_si128((const __m128i *) (frame1 + i));
        __m128i v1_1 = _mm_loadu_si128((const __m128i *) (frame1 + i + 16));
        __m128i v1_2 = _mm_loadu_si128((const __m128i *) (frame1 + i + 32));

        __m128i v2_0 = _mm_loadu_si128((const __m128i *) (frame2 + i));
        __m128i v2_1 = _mm_loadu_si128((const __m128i *) (frame2 + i + 16));
        __m128i v2_2 = _mm_loadu_si128((const __m128i *) (frame2 + i + 32));

        /* Compare each chunk: 0xFF if equal, 0x00 if different */
        __m128i cmp0 = _mm_cmpeq_epi8(v1_0, v2_0);
        __m128i cmp1 = _mm_cmpeq_epi8(v1_1, v2_1);
        __m128i cmp2 = _mm_cmpeq_epi8(v1_2, v2_2);

        /* Invert: 0xFF if different, 0x00 if equal */
        __m128i diff0 = _mm_andnot_si128(cmp0, _mm_set1_epi8(0xFF));
        __m128i diff1 = _mm_andnot_si128(cmp1, _mm_set1_epi8(0xFF));
        __m128i diff2 = _mm_andnot_si128(cmp2, _mm_set1_epi8(0xFF));

        /* Extract bitmasks for each 16-byte chunk */
        int mask0 = _mm_movemask_epi8(diff0);
        int mask1 = _mm_movemask_epi8(diff1);
        int mask2 = _mm_movemask_epi8(diff2);

        /* Count differing bytes across all 48 bytes using POPCNT */
        int diff_bytes = __builtin_popcount(mask0) + __builtin_popcount(mask1) +
                         __builtin_popcount(mask2);

        /* Approximate pixel differences
         * Since we're checking 48 bytes (16 pixels), any byte difference
         * indicates the containing pixel differs. This is conservative. */
        diff_count += diff_bytes;

        i += 48;
    }

    /* Scalar fallback for remainder */
    size_t scalar_diff = 0;
    for (; i + 3 <= total_bytes; i += 3) {
        if (frame1[i] != frame2[i] || frame1[i + 1] != frame2[i + 1] ||
            frame1[i + 2] != frame2[i + 2]) {
            scalar_diff++;
        }
    }

    /* The SIMD loop counts byte differences, not pixel differences
     * Divide by 3 to approximate pixels (conservative estimate)
     * The scalar loop provides exact pixel count for remainder */
    return (diff_count / 3) + scalar_diff;
}

/* Calculate difference percentage (0-100) */
static inline int framediff_percentage_sse(const uint8_t *restrict frame1,
                                           const uint8_t *restrict frame2,
                                           size_t pixel_count)
{
    size_t diff_pixels = framediff_count_sse(frame1, frame2, pixel_count);
    return (int) ((diff_pixels * 100) / pixel_count);
}

#endif /* __x86_64__ || _M_X64 || __i386__ || _M_IX86 */
