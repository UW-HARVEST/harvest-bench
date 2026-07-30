/* Glue TU for the upstream lz4 block-API test program (fuzzer.c).
 *
 * fuzzer.c is included verbatim with its main() renamed away, which gives
 * this TU access to its static test functions. The entry points below fix
 * every source of randomness (explicit seed, no time limit) so runs are
 * deterministic. Failures inside FUZ_* call exit() with a nonzero status;
 * the harness runs each GoogleTest case in its own process, so that maps
 * cleanly to a single failed test.
 *
 * This file must stay C: it includes upstream sources, which are not valid
 * C++. It is therefore kept as thin as possible — the scenario table and the
 * banner parsing live in fuzzer_scenarios.cc, and the recording machinery in
 * scenario_record.cc.
 *
 * FUZ_unitTests() packs 17 independent scenarios into one function and aborts
 * at the first failure. To report them individually (see ../upstream_tests.cc)
 * this TU intercepts the progress banners upstream already prints: defining
 * `fprintf` as a macro before including fuzzer.c makes upstream's
 * `#define DISPLAY(...) fprintf(stdout, __VA_ARGS__)` route through the hook,
 * because the macro body is rescanned at each use site. fuzzer.c itself is not
 * modified.
 */
#include <stdarg.h>
#include <stdio.h>

#include "scenario_record.h"

/* Implemented in fuzzer_scenarios.cc. */
void harvest_fuzzer_on_banner(const char *text);
void harvest_fuzzer_reset_banners(void);

static int harvest_display_hook(FILE *stream, const char *fmt, ...);

#define fprintf harvest_display_hook
#define main    lz4_upstream_fuzzer_unused_main
#include "fuzzer.c"
#undef main
#undef fprintf

static int harvest_display_hook(FILE *stream, const char *fmt, ...)
{
    char buf[4096];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    harvest_fuzzer_on_banner(buf);
    fputs(buf, stream);
    return n;
}

static int g_unit_level;

static int run_unit_tests(void)
{
    harvest_fuzzer_reset_banners();
    g_displayLevel = 3;   /* emit the banners the hook keys off */
    FUZ_unitTests(g_unit_level);
    return 0;
}

/* Deterministic unit-test section. Upstream runs this only when no seed is
 * given on the command line (which also randomizes the fuzz seed); calling
 * it directly keeps it deterministic. Uses its own internal fixed seed.
 * Executed once per (tag, library) and recorded per scenario. */
int harvest_fuzzer_unit_tests_recorded(int compressionLevel, const char *tag)
{
    /* Unbuffered: a banner must reach the record before its scenario runs,
     * otherwise a crash would lose it (stdout to a pipe is block-buffered). */
    setvbuf(stdout, NULL, _IONBF, 0);
    g_unit_level = compressionLevel;
    return sr_run_once(tag, run_unit_tests, (void *)(size_t)LZ4_versionNumber);
}

/* Fixed-seed fuzz loop. compressibilityPct matches upstream -P# (percent).
 * duration_s = 0 disables the wall-clock cutoff so the cycle count alone
 * determines the workload. */
int harvest_fuzzer_fuzz(unsigned seed, unsigned nbCycles, int compressibilityPct)
{
    g_displayLevel = 3; /* banners let a failure pinpoint the cycle */
    return FUZ_test((U32)seed, (U32)nbCycles, 0,
                    (double)compressibilityPct / 100.0, 0);
}
