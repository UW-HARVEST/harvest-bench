/* Glue TU for the upstream zstd zstreamtest.c (streaming API tests).
 * Upstream runs basicUnitTests with constant seed 0 "for predictability";
 * the fuzz loops take an explicit seed.
 *
 * Its basicUnitTests() packs ~84 scenarios into one function and aborts at the
 * first failure, so the banners it prints are intercepted and recorded to
 * report each scenario separately — see glue_fuzzer.c for how the `fprintf`
 * macro hook works, and scenario_record.h for the recording model.
 * zstreamtest.c itself is not modified.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scenario_record.h"

/* Implemented in scenarios.cc. */
void harvest_zstream_on_banner(const char *text);

static int harvest_zstream_hook(FILE *stream, const char *fmt, ...);

#define fprintf harvest_zstream_hook
#define main    zstd_upstream_zstream_unused_main
#include "zstreamtest.c"
#undef main
#undef fprintf

static int parse_scenario_id(const char *s)
{
    const char *p;
    char *end;
    long v;
    if (strncmp(s, "test", 4) != 0) return -1;
    p = s + 4;
    while (*p == ' ') p++;
    v = strtol(p, &end, 10);
    if (end == p) return -1;
    while (*end == ' ') end++;
    if (*end != ':') return -1;
    return (int)v;
}

static int harvest_zstream_hook(FILE *stream, const char *fmt, ...)
{
    char buf[8192];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    harvest_zstream_on_banner(buf);

    fputs(buf, stream);
    return n;
}

static int g_zstream_big;

static int run_basic(void)
{
    g_displayLevel = 3;   /* emit the banners the hook keys off */
    return basicUnitTests(0, 0.50, g_zstream_big);
}

int harvest_zstream_basic_recorded(int bigTests)
{
    g_zstream_big = bigTests;
    return sr_run_once("zstream_basic", run_basic,
                       (void *)(size_t)ZSTD_versionNumber);
}

int harvest_zstream_fuzz_old(unsigned seed, unsigned nbTests, int bigTests)
{
    g_displayLevel = 3; /* show "test N :" banners so a failure pinpoints the sub-test */
    return fuzzerTests((U32)seed, nbTests, 0, 0.50, bigTests);
}

int harvest_zstream_fuzz_new(unsigned seed, int nbTests, int bigTests)
{
    g_displayLevel = 3; /* show "test N :" banners so a failure pinpoints the sub-test */
    return fuzzerTests_newAPI((U32)seed, nbTests, 0, 0.50, bigTests);
}
