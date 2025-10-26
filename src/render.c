/* SPDX-License-Identifier: GPL-2.0 */
/*
 * kitty-doom is freely redistributable under the GNU GPL. See the file
 * "LICENSE" for information on usage and redistribution of this file.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "base64.h"
#include "kitty-doom.h"

#if defined(__aarch64__) || defined(__ARM_NEON)
#include "arch/neon-framediff.h"
#endif

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
    defined(_M_IX86)
#include "arch/sse-framediff.h"
#endif

#define WIDTH 320
#define HEIGHT 200
#define FRAME_SKIP_THRESHOLD 1 /* Skip update if < 1% pixels changed */

struct renderer {
    int screen_rows, screen_cols;
    long kitty_id;
    int frame_number;
    size_t encoded_buffer_size;
    uint8_t *prev_frame; /* Previous frame for diff detection */
    char encoded_buffer[];
};

renderer_t *renderer_create(int screen_rows, int screen_cols)
{
    /* Calculate base64 encoded size (4 * ceil(input_size / 3)) */
    const size_t bitmap_size = WIDTH * HEIGHT * 3;
    const size_t encoded_buffer_size = 4 * ((bitmap_size + 2) / 3) + 1;

    renderer_t *r = malloc(sizeof(renderer_t) + encoded_buffer_size);
    if (!r)
        return NULL;

    /* Allocate previous frame buffer for diff detection */
    r->prev_frame = malloc(bitmap_size);
    if (!r->prev_frame) {
        free(r);
        return NULL;
    }
    memset(r->prev_frame, 0, bitmap_size);

    *r = (renderer_t) {
        .screen_rows = screen_rows,
        .screen_cols = screen_cols,
        .frame_number = 0,
        .encoded_buffer_size = encoded_buffer_size,
        .kitty_id = 0,               /* Will be set below */
        .prev_frame = r->prev_frame, /* Already allocated above */
    };

    /* Generate random image ID for Kitty protocol */
    srand(time(NULL));
    r->kitty_id = rand();

    /* Set the window title */
    printf("\033]21;Kitty DOOM\033\\");

    /* Clear the screen and move cursor to home */
    printf("\033[2J\033[H");
    fflush(stdout);

    /* Log the active base64 implementation */
    fprintf(stderr, "Base64 implementation: %s\n", base64_get_impl_name());

    return r;
}

void renderer_destroy(renderer_t *restrict r)
{
    if (!r)
        return;

    /* Delete the Kitty graphics image */
    printf("\033_Ga=d,i=%ld;\033\\", r->kitty_id);
    fflush(stdout);

    /* Move cursor to home and clear screen */
    printf("\033[H\033[2J");
    fflush(stdout);

    /* Reset the window title */
    printf("\033]21\033\\");
    fflush(stdout);

    /* Free allocated buffers */
    free(r->prev_frame);
    free(r);
}

void renderer_render_frame(renderer_t *restrict r,
                           const unsigned char *restrict rgb24_frame)
{
    if (!r || !rgb24_frame)
        return;

    /* rgb24_frame is already in RGB24 format from doom_get_framebuffer(3) */
    const size_t bitmap_size = WIDTH * HEIGHT * 3;

    /* Frame differencing disabled - important for menu responsiveness
     * Since we're using a=T (full transmit) mode for Ghostty compatibility
     * and have 35 FPS limiting, skipping frames causes more issues than it solves
     */

    /* Encode RGB data to base64 */
    size_t encoded_size =
        base64_encode_auto((const uint8_t *) rgb24_frame, bitmap_size,
                           (uint8_t *) r->encoded_buffer);
    r->encoded_buffer[encoded_size] = '\0';

    /* Send Kitty Graphics Protocol escape sequence with base64 data */
    /* Using Kitty mode (required for Kitty terminal) */
    const size_t chunk_size = 4096;

    /* For Ghostty: delete old image before transmitting new one (except frame 0) */
    if (r->frame_number > 0) {
        printf("\033[H\033_Ga=d,i=%ld;\033\\", r->kitty_id);
        fflush(stdout);
    } else {
        printf("\033[H");
        fflush(stdout);
    }

    for (size_t encoded_offset = 0; encoded_offset < encoded_size;) {
        bool more_chunks = (encoded_offset + chunk_size) < encoded_size;

        if (encoded_offset == 0) {
            /* First chunk includes all image metadata - use a=T for all frames
             * (Ghostty doesn't support a=f animation commands properly)
             */
            printf("\033_Ga=T,i=%ld,f=24,s=%d,v=%d,q=2,c=%d,r=%d,m=%d;",
                   r->kitty_id, WIDTH, HEIGHT, r->screen_cols,
                   r->screen_rows, more_chunks ? 1 : 0);
        } else {
            /* Continuation chunks */
            printf("\033_Gm=%d;", more_chunks ? 1 : 0);
        }

        /* Transfer payload */
        const size_t this_size =
            more_chunks ? chunk_size : encoded_size - encoded_offset;
        fwrite(r->encoded_buffer + encoded_offset, 1, this_size, stdout);
        printf("\033\\");
        fflush(stdout);

        encoded_offset += this_size;
    }

    /* On first frame, add newline to move cursor below image */
    if (r->frame_number == 0) {
        printf("\r\n");
        fflush(stdout);
    }

    /* Update previous frame buffer for next diff */
    memcpy(r->prev_frame, rgb24_frame, bitmap_size);

    r->frame_number++;
}
