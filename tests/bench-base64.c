/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Base64 encoding tests and benchmarks
 * - Correctness tests (RFC 4648 conformance)
 * - Performance benchmarks
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../src/base64.h"

/* Correctness Tests */

/* Test vector structure */
typedef struct {
    const char *input;
    size_t input_len;
    const char *expected;
} test_vector_t;

/* RFC 4648 test vectors */
static const test_vector_t test_vectors[] = {
    /* Empty string */
    {"", 0, ""},

    /* Single byte */
    {"f", 1, "Zg=="},

    /* Two bytes */
    {"fo", 2, "Zm8="},

    /* Three bytes (no padding) */
    {"foo", 3, "Zm9v"},

    /* Four bytes */
    {"foob", 4, "Zm9vYg=="},

    /* Five bytes */
    {"fooba", 5, "Zm9vYmE="},

    /* Six bytes (multiple of 3) */
    {"foobar", 6, "Zm9vYmFy"},

    /* Longer strings */
    {"The quick brown fox jumps over the lazy dog", 43,
     "VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZw=="},
};

static const size_t num_test_vectors =
    sizeof(test_vectors) / sizeof(test_vectors[0]);

/* Test a single implementation */
static bool test_impl(const char *name,
                      size_t (*encode_func)(const uint8_t *, size_t, uint8_t *))
{
    bool all_passed = true;
    uint8_t output[1024];

    printf("Testing %s implementation:\n", name);

    for (size_t i = 0; i < num_test_vectors; i++) {
        const test_vector_t *tv = &test_vectors[i];

        memset(output, 0, sizeof(output));
        size_t output_len =
            encode_func((const uint8_t *) tv->input, tv->input_len, output);

        size_t expected_len = strlen(tv->expected);
        if (output_len != expected_len) {
            printf(
                "  [FAIL] Test %zu: length mismatch (got %zu, expected %zu)\n",
                i, output_len, expected_len);
            all_passed = false;
            continue;
        }

        if (memcmp(output, tv->expected, output_len) != 0) {
            printf("  [FAIL] Test %zu: output mismatch\n", i);
            printf("    Input:    \"%.*s\" (%zu bytes)\n", (int) tv->input_len,
                   tv->input, tv->input_len);
            printf("    Expected: \"%s\"\n", tv->expected);
            printf("    Got:      \"%.*s\"\n", (int) output_len, output);
            all_passed = false;
            continue;
        }

        printf("  [PASS] Test %zu: \"%.*s\" -> \"%s\"\n", i,
               (int) tv->input_len, tv->input, tv->expected);
    }

    return all_passed;
}

/* Test various input lengths to check boundary conditions */
static bool test_boundary(const char *name,
                          size_t (*encode_func)(const uint8_t *,
                                                size_t,
                                                uint8_t *))
{
    bool all_passed = true;
    uint8_t input[256];
    uint8_t output[512];
    uint8_t reference[512];

    printf("\nTesting %s boundary conditions:\n", name);

    /* Initialize input with sequential bytes */
    for (size_t i = 0; i < sizeof(input); i++)
        input[i] = (uint8_t) i;

    /* Test all lengths from 0 to 100 */
    for (size_t len = 0; len <= 100; len++) {
        /* Encode with test implementation */
        memset(output, 0, sizeof(output));
        size_t output_len = encode_func(input, len, output);

        /* Encode with reference (scalar) */
        memset(reference, 0, sizeof(reference));
        size_t ref_len = base64_encode_scalar(input, len, reference);

        /* Compare lengths */
        if (output_len != ref_len) {
            printf(
                "  [FAIL] Length %zu: output length mismatch (got %zu, "
                "expected %zu)\n",
                len, output_len, ref_len);
            all_passed = false;
            continue;
        }

        /* Compare outputs */
        if (memcmp(output, reference, output_len) != 0) {
            printf("  [FAIL] Length %zu: output mismatch\n", len);
            all_passed = false;
            continue;
        }
    }

    if (all_passed)
        printf("  [PASS] All lengths 0-100 passed\n");

    return all_passed;
}

/* Test large data (like DOOM framebuffer) */
static bool test_large_data(const char *name,
                            size_t (*encode_func)(const uint8_t *,
                                                  size_t,
                                                  uint8_t *))
{
    const size_t sizes[] = {
        1024,   /* 1KB */
        4096,   /* 4KB */
        16384,  /* 16KB */
        65536,  /* 64KB */
        192000, /* DOOM framebuffer size */
    };

    bool all_passed = true;

    printf("\nTesting %s with large data:\n", name);

    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        size_t size = sizes[i];

        /* Allocate buffers */
        uint8_t *input = malloc(size);
        uint8_t *output = malloc(size * 2); /* base64 expands ~4/3 */
        uint8_t *reference = malloc(size * 2);

        if (!input || !output || !reference) {
            printf("  [FAIL] Memory allocation failed for size %zu\n", size);
            free(input);
            free(output);
            free(reference);
            all_passed = false;
            continue;
        }

        /* Fill with pseudo-random data */
        for (size_t j = 0; j < size; j++)
            input[j] = (uint8_t) (j * 17 + 42);

        /* Encode with test implementation */
        size_t output_len = encode_func(input, size, output);

        /* Encode with reference */
        size_t ref_len = base64_encode_scalar(input, size, reference);

        /* Compare */
        bool passed = (output_len == ref_len) &&
                      (memcmp(output, reference, output_len) == 0);

        if (passed) {
            printf("  [PASS] Size %zu bytes -> %zu base64 chars\n", size,
                   output_len);
        } else {
            printf("  [FAIL] Size %zu bytes\n", size);
            all_passed = false;
        }

        free(input);
        free(output);
        free(reference);
    }

    return all_passed;
}

/* Run all correctness tests */
static bool test_correctness(void)
{
    bool all_passed = true;

    printf("=== Base64 Encoding Correctness Tests ===\n\n");

    /* Test scalar implementation */
    all_passed &= test_impl("Scalar", base64_encode_scalar);
    all_passed &= test_boundary("Scalar", base64_encode_scalar);
    all_passed &= test_large_data("Scalar", base64_encode_scalar);

    printf("\n");

    /* Test NEON implementation if available */
#if defined(__aarch64__) || defined(__ARM_NEON)
    all_passed &= test_impl("NEON", base64_encode_neon);
    all_passed &= test_boundary("NEON", base64_encode_neon);
    all_passed &= test_large_data("NEON", base64_encode_neon);

    printf("\n");
#else
    printf("NEON not available on this platform\n\n");
#endif

    /* Test automatic selection */
    printf("Active implementation: %s\n", base64_get_impl_name());
    all_passed &= test_impl("Auto", base64_encode_auto);
    all_passed &= test_boundary("Auto", base64_encode_auto);
    all_passed &= test_large_data("Auto", base64_encode_auto);

    printf("\n=== Correctness Summary ===\n");
    if (all_passed) {
        printf("All tests PASSED\n");
    } else {
        printf("Some tests FAILED\n");
    }

    return all_passed;
}

/* Performance Benchmarks */

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
static bench_result_t bench_impl(const char *name,
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
static void print_res(const bench_result_t *r)
{
    printf("  %-20s %8zu bytes: ", r->name, r->input_size);
    printf("min=%6.2f us, ", r->min_ns / 1000.0);
    printf("avg=%6.2f us, ", r->avg_ns / 1000.0);
    printf("max=%6.2f us  ", r->max_ns / 1000.0);
    printf("=> %8.2f MB/s\n", r->throughput_mbps);
}

/* Print comparison table */
static void print_cmp(const bench_result_t *results, size_t count)
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

/* Run all performance benchmarks */
static void bench_perf(void)
{
    printf("\n");
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
            bench_impl("Scalar", base64_encode_scalar, size);
        print_res(&results[result_count - 1]);

#if defined(__aarch64__) || defined(__ARM_NEON)
        /* Benchmark NEON */
        results[result_count++] = bench_impl("NEON", base64_encode_neon, size);
        print_res(&results[result_count - 1]);
#endif

        /* Benchmark auto (should select best) */
        results[result_count++] = bench_impl("Auto", base64_encode_auto, size);
        print_res(&results[result_count - 1]);

        /* Print comparison */
        print_cmp(results, result_count);

        printf("\n");
    }

    /* Print summary for DOOM framebuffer size */
    printf("=== Summary for DOOM Framebuffer (192000 bytes) ===\n");

    bench_result_t scalar_result =
        bench_impl("Scalar", base64_encode_scalar, 192000);
    bench_result_t auto_result = bench_impl("Auto", base64_encode_auto, 192000);

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
}

int main(void)
{
    /* Run correctness tests first */
    bool tests_passed = test_correctness();

    if (!tests_passed) {
        fprintf(stderr,
                "\nERROR: Correctness tests failed, skipping benchmarks\n");
        return 1;
    }

    /* Run performance benchmarks */
    bench_perf();

    return 0;
}
