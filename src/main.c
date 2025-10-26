/* SPDX-License-Identifier: GPL-2.0 */
/*
 * kitty-doom is freely redistributable under the GNU GPL. See the file
 * "LICENSE" for information on usage and redistribution of this file.
 */

#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* PureDOOM implementation - bundles the DOOM engine as C code */
#define DOOM_IMPLEMENTATION
#define DOOM_IMPLEMENT_MALLOC
#define DOOM_IMPLEMENT_FILE_IO
#define DOOM_IMPLEMENT_GETTIME
#define DOOM_IMPLEMENT_GETENV
#include "PureDOOM.h"

#include "kitty-doom.h"

static const char *last_print_string = NULL;

/* Signal handling for graceful shutdown
 * IMPORTANT: Only sig_atomic_t access is allowed in signal handlers.
 * The handler sets a flag, and shutdown is handled in the main thread.
 */
static volatile sig_atomic_t signal_received = 0;

static void signal_handler(int signum)
{
    (void) signum; /* Unused parameter */
    signal_received = 1;
}

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

    /* If error occurred during initialization (exit_code != 0),
     * must terminate immediately to prevent undefined behavior.
     * The error message has already been captured by print_handler.
     */
    if (exit_code != 0) {
        if (last_print_string) {
            fprintf(stderr, "\nDOOM Error: %s\n", last_print_string);
            fflush(stderr);
        }
        /* Cannot continue after DOOM error - must exit immediately */
        exit(EXIT_FAILURE);
    }
}

/* Check if terminal supports Kitty Graphics Protocol
 * Returns: true if supported, false if unsupported and should abort
 */
static bool check_supported_term(void)
{
    const char *term = getenv("TERM");
    const char *term_program = getenv("TERM_PROGRAM");

    /* Quick check: known compatible terminals via environment variables */
    if (term && strstr(term, "kitty"))
        return true; /* Kitty sets TERM=xterm-kitty */

    if (term_program) {
        if (!strcmp(term_program, "ghostty") ||
            !strcmp(term_program, "WezTerm"))
            return true;
    }

    /* Active probe: Query Kitty Graphics Protocol support */
    fprintf(stderr,
            "Probing terminal for Kitty Graphics Protocol support...\n");

    /* Save terminal state */
    struct termios old_tio, new_tio;
    if (tcgetattr(STDIN_FILENO, &old_tio) != 0) {
        fprintf(stderr, "Warning: Cannot probe terminal (tcgetattr failed)\n");
        goto fallback_warning;
    }

    /* Set non-canonical mode for reading response */
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);
    new_tio.c_cc[VMIN] = 0;
    new_tio.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

    /* Send Kitty Graphics query: probe 1x1 pixel capability
     * Format: \033_Gi=31,s=1,v=1,a=q,t=d,f=24;AAAA\033\\
     * If supported, terminal responds with: \033_Gi=31;OK\033\\ or similar
     */
    printf("\033_Gi=31,s=1,v=1,a=q,t=d,f=24;AAAA\033\\");
    fflush(stdout);

    /* Wait for response with timeout (200ms) */
    struct pollfd pfd = {
        .fd = STDIN_FILENO,
        .events = POLLIN,
    };

    bool supported = false;
    if (poll(&pfd, 1, 200) > 0 && (pfd.revents & POLLIN)) {
        char buf[256];
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            /* Check for Kitty Graphics response */
            if (strstr(buf, "\033_Gi=31")) {
                supported = true;
                fprintf(stderr, "Terminal supports Kitty Graphics Protocol\n");
            }
        }
    }

    /* Restore terminal state */
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);

    if (supported)
        return true;

fallback_warning:
    /* Terminal doesn't support Kitty Graphics Protocol - abort */
    fprintf(
        stderr,
        "\n"
        "ERROR: Terminal does not support Kitty Graphics Protocol\n"
        "       TERM=%s\n"
        "       TERM_PROGRAM=%s\n"
        "\n"
        "Kitty-DOOM requires a terminal with Kitty Graphics Protocol support.\n"
        "\n"
        "Recommended terminals:\n"
        "  - Kitty:   https://sw.kovidgoyal.net/kitty/\n"
        "  - Ghostty: https://ghostty.org\n"
        "\n"
        "Running in unsupported terminals will cause display corruption.\n"
        "\n",
        term ? term : "(not set)", term_program ? term_program : "(not set)");
    fflush(stderr);

    return false; /* Abort execution */
}

int main(int argc, char **argv)
{
    /* Signal handlers are installed for graceful shutdown */
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, "Failed to install SIGINT handler\n");
        return EXIT_FAILURE;
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        fprintf(stderr, "Failed to install SIGTERM handler\n");
        return EXIT_FAILURE;
    }

    /* Check terminal compatibility before initialization */
    if (!check_supported_term())
        return EXIT_FAILURE;

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

    /* doom_init may have triggered an exit */
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

    /* Main game loop - 35 FPS (28.57ms per frame) */
    const long frame_time_ns = 28571428; /* 1000ms / 35fps = 28.571ms */
    struct timespec frame_start, frame_end, sleep_time;

    while (input_is_running(input) && !exit_requested && !signal_received) {
        clock_gettime(CLOCK_MONOTONIC, &frame_start);

        doom_update();

        /* RGB24 format is obtained directly from PureDOOM */
        const unsigned char *frame = doom_get_framebuffer(3);
        renderer_render_frame(r, frame);

        /* Frame timing: sleep to maintain 35 FPS */
        clock_gettime(CLOCK_MONOTONIC, &frame_end);
        long elapsed_ns = (frame_end.tv_sec - frame_start.tv_sec) * 1000000000L +
                         (frame_end.tv_nsec - frame_start.tv_nsec);
        long sleep_ns = frame_time_ns - elapsed_ns;

        if (sleep_ns > 0) {
            sleep_time.tv_sec = 0;
            sleep_time.tv_nsec = sleep_ns;
            nanosleep(&sleep_time, NULL);
        }
    }

    /* Input thread is requested to exit before cleanup
     * This call is unconditional to handle signals arriving after loop check
     * input_request_exit() is idempotent, thus safe to call multiple times
     */
    input_request_exit(input);

    /* Resources are cleaned up in reverse order */
    renderer_destroy(r);
    input_destroy(input);
    os_destroy(os);

    if (exit_requested && last_print_string)
        printf("%s\n", last_print_string);

    return exit_code_global == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
