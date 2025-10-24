/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Base64 encoding correctness tests
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/base64.h"

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

    /* Binary data (all bytes 0-255) would be tested separately */
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
static bool test_boundary_conditions(const char *name,
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

int main(void)
{
    bool all_passed = true;

    printf("=== Base64 Encoding Correctness Tests ===\n\n");

    /* Test scalar implementation */
    all_passed &= test_impl("Scalar", base64_encode_scalar);
    all_passed &= test_boundary_conditions("Scalar", base64_encode_scalar);
    all_passed &= test_large_data("Scalar", base64_encode_scalar);

    printf("\n");

    /* Test NEON implementation if available */
#if defined(__aarch64__) || defined(__ARM_NEON)
    all_passed &= test_impl("NEON", base64_encode_neon);
    all_passed &= test_boundary_conditions("NEON", base64_encode_neon);
    all_passed &= test_large_data("NEON", base64_encode_neon);

    printf("\n");
#else
    printf("NEON not available on this platform\n\n");
#endif

    /* Test automatic selection */
    printf("Active implementation: %s\n", base64_get_impl_name());
    all_passed &= test_impl("Auto", base64_encode_auto);
    all_passed &= test_boundary_conditions("Auto", base64_encode_auto);
    all_passed &= test_large_data("Auto", base64_encode_auto);

    printf("\n=== Summary ===\n");
    if (all_passed) {
        printf("All tests PASSED\n");
        return 0;
    }

    printf("Some tests FAILED\n");
    return 1;
}
