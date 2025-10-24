/* SPDX-License-Identifier: GPL-2.0 */
/*
 * kitty-doom is freely redistributable under the GNU GPL. See the file
 * "LICENSE" for information on usage and redistribution of this file.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PureDOOM implementation - bundles the DOOM engine as C code */
#define DOOM_IMPLEMENTATION
#define DOOM_IMPLEMENT_MALLOC
#define DOOM_IMPLEMENT_FILE_IO
#define DOOM_IMPLEMENT_GETTIME
#define DOOM_IMPLEMENT_GETENV
#include "PureDOOM.h"

#include "kitty-doom.h"

static const char *last_print_string = NULL;

static void print_handler(const char *s)
{
    /* Track the last print string to display as an error message when an exit
     * call is received. Linefeeds following the error message are ignored.
     */
    if (*s != '\n')
        last_print_string = s;
}

static int exit_code_global = -1; /* -1 means "not exited" */
static bool exit_requested = false;

static void exit_handler(int exit_code)
{
    exit_code_global = exit_code;
    exit_requested = true;
}

int main(int argc, char **argv)
{
    os_t *os = os_create();
    if (!os) {
        fprintf(stderr, "Failed to initialize OS layer\n");
        return EXIT_FAILURE;
    }

    input_t *input = input_create();
    if (!input) {
        fprintf(stderr, "Failed to initialize input\n");
        os_destroy(os);
        return EXIT_FAILURE;
    }

    doom_set_print(print_handler);
    doom_set_exit(exit_handler);
    doom_init(argc, argv, 0);

    /* Check if doom_init triggered an exit */
    if (exit_requested) {
        if (last_print_string)
            printf("%s\n", last_print_string);
        input_destroy(input);
        os_destroy(os);
        return exit_code_global == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    const int_pair_t cells = input_get_screen_cells(input);
    renderer_t *r = renderer_create(cells.first, cells.second);
    if (!r) {
        fprintf(stderr, "Failed to initialize renderer\n");
        input_destroy(input);
        os_destroy(os);
        return EXIT_FAILURE;
    }

    /* Main game loop */
    while (input_is_running(input) && !exit_requested) {
        doom_update();

        /* Get RGB24 format directly from PureDOOM */
        const unsigned char *frame = doom_get_framebuffer(3);
        renderer_render_frame(r, frame);
    }

    /* Cleanup resources in reverse order */
    renderer_destroy(r);
    input_destroy(input);
    os_destroy(os);

    if (exit_requested && last_print_string)
        printf("%s\n", last_print_string);

    return exit_code_global == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
