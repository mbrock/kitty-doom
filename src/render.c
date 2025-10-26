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

#define WIDTH 320
#define HEIGHT 200

struct renderer {
    int screen_rows, screen_cols;
    long kitty_id;
    int frame_number;
    size_t encoded_buffer_size;
    bool use_animation; /* true for Kitty (a=f), false for others (a=T) */
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

    /* Detect terminal type to choose rendering mode */
    const char *term = getenv("TERM");
    const char *term_program = getenv("TERM_PROGRAM");
    bool use_animation = false; /* Default: use a=T (transmit) mode */

    /* Only use animation mode (a=f) for actual Kitty terminal */
    if (term && strstr(term, "kitty")) {
        use_animation = true;
        fprintf(stderr, "Detected Kitty terminal - using animation mode\n");
    } else {
        fprintf(stderr, "Using compatibility mode for %s\n",
                term_program ? term_program : (term ? term : "unknown"));
    }

    *r = (renderer_t) {
        .screen_rows = screen_rows,
        .screen_cols = screen_cols,
        .frame_number = 0,
        .encoded_buffer_size = encoded_buffer_size,
        .kitty_id = 0,            /* Will be set below */
        .use_animation = use_animation,
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

    free(r);
}

void renderer_render_frame(renderer_t *restrict r,
                           const unsigned char *restrict rgb24_frame)
{
    if (!r || !rgb24_frame)
        return;

    /* rgb24_frame is already in RGB24 format from doom_get_framebuffer(3) */
    const size_t bitmap_size = WIDTH * HEIGHT * 3;

    /* On first frame, ensure cursor is at home position */
    if (r->frame_number == 0) {
        printf("\033[H");
        fflush(stdout);
    }

    /* Encode RGB data to base64 */
    size_t encoded_size =
        base64_encode_auto((const uint8_t *) rgb24_frame, bitmap_size,
                           (uint8_t *) r->encoded_buffer);
    r->encoded_buffer[encoded_size] = '\0';

    /* Send Kitty Graphics Protocol escape sequence with base64 data */
    const size_t chunk_size = 4096;

    if (r->use_animation) {
        /* Animation mode (a=f) for Kitty terminal - efficient frame updates */
        for (size_t encoded_offset = 0; encoded_offset < encoded_size;) {
            bool more_chunks = (encoded_offset + chunk_size) < encoded_size;

            if (encoded_offset == 0) {
                /* First chunk includes all image metadata */
                if (r->frame_number == 0) {
                    /* First frame: create new image */
                    printf("\033_Ga=T,i=%ld,f=24,s=%d,v=%d,q=2,c=%d,r=%d,m=%d;",
                           r->kitty_id, WIDTH, HEIGHT, r->screen_cols,
                           r->screen_rows, more_chunks ? 1 : 0);
                } else {
                    /* Subsequent frames: use frame action */
                    printf("\033_Ga=f,r=1,i=%ld,f=24,x=0,y=0,s=%d,v=%d,m=%d;",
                           r->kitty_id, WIDTH, HEIGHT, more_chunks ? 1 : 0);
                }
            } else {
                /* Continuation chunks */
                if (r->frame_number == 0) {
                    printf("\033_Gm=%d;", more_chunks ? 1 : 0);
                } else {
                    printf("\033_Ga=f,r=1,m=%d;", more_chunks ? 1 : 0);
                }
            }

            /* Transfer payload */
            const size_t this_size =
                more_chunks ? chunk_size : encoded_size - encoded_offset;
            fwrite(r->encoded_buffer + encoded_offset, 1, this_size, stdout);
            printf("\033\\");
            fflush(stdout);

            encoded_offset += this_size;
        }

        /* For Kitty mode, animate the frame after first frame */
        if (r->frame_number > 0) {
            printf("\033_Ga=a,c=1,i=%ld;\033\\", r->kitty_id);
            fflush(stdout);
        }
    } else {
        /* Compatibility mode (a=T) for Ghostty and other terminals */
        /* Delete old image before transmitting new one (except frame 0) */
        if (r->frame_number > 0) {
            printf("\033[H\033_Ga=d,i=%ld;\033\\", r->kitty_id);
            fflush(stdout);
        }

        for (size_t encoded_offset = 0; encoded_offset < encoded_size;) {
            bool more_chunks = (encoded_offset + chunk_size) < encoded_size;

            if (encoded_offset == 0) {
                /* Use a=T (transmit) for all frames */
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
    }

    /* On first frame, add newline to move cursor below image */
    if (r->frame_number == 0) {
        printf("\r\n");
        fflush(stdout);
    }

    r->frame_number++;
}
