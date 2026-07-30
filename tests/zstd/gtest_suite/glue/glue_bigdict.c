/* Glue TU for bigdict.c (compression with a >=1 GB dictionary window). */
#define main zstd_upstream_bigdict_unused_main
#include "bigdict.c"
#undef main

int harvest_bigdict(void)
{
    const char* argv[] = { "bigdict" };
    return zstd_upstream_bigdict_unused_main(1, argv);
}
