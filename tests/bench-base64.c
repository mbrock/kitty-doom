/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Base64 encoding performance benchmark
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../src/base64.h"

/* Benchmark configuration */
#define WARMUP_ITERATIONS 10
#define BENCHMARK_ITERATIONS 1000

/* Test data sizes */
static const size_t test_sizes[] = {
    1024,   /* 1KB - small data */
    4096,   /* 4KB - typical chunk */
    16384,  /* 16KB - medium */
    65536,  /* 64KB - large */
    192000, /* DOOM framebuffer (320x200x3) */
};

static const size_t num_test_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);

/* High-resolution timer */
static inline uint64_t get_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000000ULL + (uint64_t) ts.tv_nsec;
}

/* Benchmark result structure */
typedef struct {
    const char *name;
    size_t input_size;
    uint64_t min_ns;
    uint64_t avg_ns;
    uint64_t max_ns;
    double throughput_mbps; /* MB/s */
} bench_result_t;

/* Run benchmark for a single implementation and size */
static bench_result_t benchmark_impl(const char *name,
                                     size_t (*encode_func)(const uint8_t *,
                                                           size_t,
                                                           uint8_t *),
                                     size_t size)
{
    bench_result_t result = {
        .name = name,
        .input_size = size,
        .min_ns = UINT64_MAX,
        .avg_ns = 0,
        .max_ns = 0,
        .throughput_mbps = 0.0,
    };

    /* Allocate buffers */
    uint8_t *input = malloc(size);
    uint8_t *output = malloc(size * 2); /* base64 expands ~4/3 */

    if (!input || !output) {
        fprintf(stderr, "Memory allocation failed\n");
        free(input);
        free(output);
        return result;
    }

    /* Fill input with pseudo-random data */
    for (size_t i = 0; i < size; i++)
        input[i] = (uint8_t) (i * 17 + 42);

    /* Warmup iterations (to populate cache) */
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        encode_func(input, size, output);
    }

    /* Benchmark iterations */
    uint64_t total_ns = 0;

    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        uint64_t start = get_time_ns();
        encode_func(input, size, output);
        uint64_t end = get_time_ns();

        uint64_t elapsed = end - start;
        total_ns += elapsed;

        if (elapsed < result.min_ns)
            result.min_ns = elapsed;
        if (elapsed > result.max_ns)
            result.max_ns = elapsed;
    }

    result.avg_ns = total_ns / BENCHMARK_ITERATIONS;

    /* Calculate throughput in MB/s */
    double seconds = (double) result.avg_ns / 1000000000.0;
    double mb = (double) size / (1024.0 * 1024.0);
    result.throughput_mbps = mb / seconds;

    free(input);
    free(output);

    return result;
}

/* Print benchmark result */
static void print_result(const bench_result_t *r)
{
    printf("  %-20s %8zu bytes: ", r->name, r->input_size);
    printf("min=%6.2f us, ", r->min_ns / 1000.0);
    printf("avg=%6.2f us, ", r->avg_ns / 1000.0);
    printf("max=%6.2f us  ", r->max_ns / 1000.0);
    printf("=> %8.2f MB/s\n", r->throughput_mbps);
}

/* Print comparison table */
static void print_comparison(const bench_result_t *results, size_t count)
{
    if (count == 0)
        return;

    /* Find baseline (scalar) */
    const bench_result_t *baseline = NULL;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(results[i].name, "Scalar") == 0) {
            baseline = &results[i];
            break;
        }
    }

    if (!baseline)
        return;

    printf("\n  Speedup relative to Scalar baseline:\n");

    for (size_t i = 0; i < count; i++) {
        double speedup = results[i].throughput_mbps / baseline->throughput_mbps;
        printf("    %-20s %.2fx\n", results[i].name, speedup);
    }
}

int main(void)
{
    printf("=== Base64 Encoding Performance Benchmark ===\n");
    printf("Configuration:\n");
    printf("  Warmup iterations:    %d\n", WARMUP_ITERATIONS);
    printf("  Benchmark iterations: %d\n", BENCHMARK_ITERATIONS);
    printf("\n");

    /* Test each size */
    for (size_t s = 0; s < num_test_sizes; s++) {
        size_t size = test_sizes[s];
        bench_result_t results[16];
        size_t result_count = 0;

        printf("Testing with %zu bytes (%.2f KB):\n", size, size / 1024.0);

        /* Benchmark scalar */
        results[result_count++] =
            benchmark_impl("Scalar", base64_encode_scalar, size);
        print_result(&results[result_count - 1]);

#if defined(__aarch64__) || defined(__ARM_NEON)
        /* Benchmark NEON */
        results[result_count++] =
            benchmark_impl("NEON", base64_encode_neon, size);
        print_result(&results[result_count - 1]);
#endif

        /* Benchmark auto (should select best) */
        results[result_count++] =
            benchmark_impl("Auto", base64_encode_auto, size);
        print_result(&results[result_count - 1]);

        /* Print comparison */
        print_comparison(results, result_count);

        printf("\n");
    }

    /* Print summary for DOOM framebuffer size */
    printf("=== Summary for DOOM Framebuffer (192000 bytes) ===\n");

    bench_result_t scalar_result =
        benchmark_impl("Scalar", base64_encode_scalar, 192000);
    bench_result_t auto_result =
        benchmark_impl("Auto", base64_encode_auto, 192000);

    printf("Active implementation: %s\n", base64_get_impl_name());
    printf("\n");

    printf("Scalar baseline:\n");
    printf("  Average time:  %.2f us per frame\n",
           scalar_result.avg_ns / 1000.0);
    printf("  Throughput:    %.2f MB/s\n", scalar_result.throughput_mbps);
    printf("\n");

    printf("Optimized (%s):\n", base64_get_impl_name());
    printf("  Average time:  %.2f us per frame\n", auto_result.avg_ns / 1000.0);
    printf("  Throughput:    %.2f MB/s\n", auto_result.throughput_mbps);
    printf("  Speedup:       %.2fx\n",
           auto_result.throughput_mbps / scalar_result.throughput_mbps);
    printf("\n");

    /* Calculate percentage of frame time (35 FPS = 28.57 ms/frame) */
    double frame_time_ms = 28.57;
    double scalar_pct =
        (scalar_result.avg_ns / 1000000.0) / frame_time_ms * 100.0;
    double auto_pct = (auto_result.avg_ns / 1000000.0) / frame_time_ms * 100.0;

    printf("Frame time budget (35 FPS = 28.57 ms/frame):\n");
    printf("  Scalar:     %.2f%% of frame time\n", scalar_pct);
    printf("  Optimized:  %.2f%% of frame time\n", auto_pct);
    printf("  Saved:      %.2f%% of frame time\n", scalar_pct - auto_pct);

    return 0;
}
