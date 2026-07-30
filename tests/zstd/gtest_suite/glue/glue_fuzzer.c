/* Glue TU for the upstream zstd fuzzer.c (single-shot / block-level API tests).
 * The upstream file is included verbatim with main() renamed; deterministic
 * entry points are exposed for the gtest wrappers. Upstream itself runs
 * basicUnitTests with constant seed 0 "for predictability".
 *
 * basicUnitTests() packs ~205 independent scenarios into one function and
 * aborts at the first failure. To report them individually (see
 * ../upstream_tests.cc) this TU intercepts the `test%3i : ...` banners
 * upstream already prints and records scenario boundaries; see
 * scenario_record.h. Interception works by defining `fprintf` as a macro
 * before including fuzzer.c: upstream's `#define DISPLAY(...)
 * fprintf(stderr, __VA_ARGS__)` is rescanned at each use site, so every
 * DISPLAY call routes through the hook. fuzzer.c itself is not modified.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scenario_record.h"

/* Implemented in scenarios.cc. */
void harvest_fuzzer_on_banner(const char *text);

static int harvest_display_hook(FILE *stream, const char *fmt, ...);

#define fprintf harvest_display_hook
#define main    zstd_upstream_fuzzer_unused_main
#include "fuzzer.c"
#undef main
#undef fprintf

static int harvest_display_hook(FILE *stream, const char *fmt, ...)
{
    char buf[8192];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    harvest_fuzzer_on_banner(buf);

    fputs(buf, stream);
    return n;
}

static int run_basic(void)
{
    g_displayLevel = 3;   /* emit the banners the hook keys off */
    return basicUnitTests(0, 0.50);
}

/* Executes basicUnitTests once per library under test and records each
 * scenario; later calls reuse the record. */
int harvest_fuzzer_basic_recorded(void)
{
    return sr_run_once("fuzzer_basic", run_basic, (void *)(size_t)ZSTD_versionNumber);
}

int harvest_fuzzer_long(void)
{
    g_displayLevel = 3; /* show "test N :" banners so a failure pinpoints the sub-test */
    return longUnitTests(0, 0.50);
}

int harvest_fuzzer_fuzz(unsigned seed, unsigned nbTests, int bigTests)
{
    g_displayLevel = 3; /* show "test N :" banners so a failure pinpoints the sub-test */
    return fuzzerTests((U32)seed, nbTests, 0, 0 /* no wall-clock cutoff */,
                       0.50, bigTests);
}
