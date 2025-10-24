/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Base64 encoding with runtime CPU feature detection
 */

#include "base64.h"

#include <stdbool.h>
#include <stdio.h>

/* Include architecture-specific implementations */
#include "arch/neon-base64.h"

#if defined(__aarch64__) || defined(__arm__)
/* ARM/ARM64: Check for NEON support */

#if defined(__linux__)
#include <sys/auxv.h>

#ifndef HWCAP_NEON
#define HWCAP_NEON (1 << 12)
#endif

#ifndef HWCAP_ASIMD
#define HWCAP_ASIMD (1 << 1)
#endif

static bool cpu_has_neon(void)
{
    unsigned long hwcaps = getauxval(AT_HWCAP);

#if defined(__aarch64__)
    /* AArch64: NEON (Advanced SIMD) is mandatory */
    return (hwcaps & HWCAP_ASIMD) != 0;
#else
    /* ARMv7: Check NEON feature bit */
    return (hwcaps & HWCAP_NEON) != 0;
#endif
}

#elif defined(__APPLE__)
#include <sys/sysctl.h>

static bool cpu_has_neon(void)
{
#if defined(__aarch64__)
    /* Apple Silicon (M1/M2/M3): NEON is always available */
    return true;
#else
    /* iOS ARMv7: Check hw.optional.neon */
    int has_neon = 0;
    size_t len = sizeof(has_neon);
    if (sysctlbyname("hw.optional.neon", &has_neon, &len, NULL, 0) == 0)
        return has_neon != 0;
    return false;
#endif
}

#else
/* Other platforms: assume NEON is available if compiled with NEON support */
static bool cpu_has_neon(void)
{
#if defined(__ARM_NEON)
    return true;
#else
    return false;
#endif
}
#endif /* __linux__ / __APPLE__ */

#else
/* Non-ARM platforms: no NEON */
static bool cpu_has_neon(void)
{
    return false;
}
#endif /* __aarch64__ || __arm__ */

/* Runtime selection of best implementation */
static base64_encode_func_t select_best_impl(void)
{
    static bool initialized = false;
    static base64_encode_func_t best_impl = NULL;

    if (!initialized) {
        if (cpu_has_neon()) {
            /* Use NEON SIMD implementation (simdutf algorithm) */
            best_impl = base64_encode_neon;
        } else {
            /* Fallback to basic scalar */
            best_impl = base64_encode_scalar;
        }
        initialized = true;
    }

    return best_impl;
}

/* Unified API: automatically selects best implementation */
size_t base64_encode_auto(const uint8_t *restrict input,
                          size_t input_len,
                          uint8_t *restrict output)
{
    base64_encode_func_t impl = select_best_impl();
    return impl(input, input_len, output);
}

/* Get the name of the active implementation (for debugging) */
const char *base64_get_impl_name(void)
{
    base64_encode_func_t impl = select_best_impl();

    if (impl == base64_encode_neon)
        return "NEON";
    if (impl == base64_encode_scalar)
        return "Scalar";
    return "Unknown";
}
