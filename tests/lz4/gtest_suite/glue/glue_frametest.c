/* Glue TU for the upstream lz4 frame-API test program (frametest.c).
 * Same pattern as glue_fuzzer.c: verbatim include, deterministic entries.
 */
#define main lz4_upstream_frametest_unused_main
#include "frametest.c"
#undef main

int harvest_frametest_unit_tests(unsigned seed, int compressibilityPct)
{
    displayLevel = 3; /* show per-test banners so a failure pinpoints the sub-test */
    return unitTests((U32)seed, (double)compressibilityPct / 100.0);
}

int harvest_frametest_fuzz(unsigned seed, unsigned nbTests, int compressibilityPct)
{
    displayLevel = 3; /* show per-test banners so a failure pinpoints the sub-test */
    return fuzzerTests((U32)seed, nbTests, 0,
                       (double)compressibilityPct / 100.0, 0);
}
