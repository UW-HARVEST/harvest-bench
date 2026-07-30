/* Glue TU for largeDictionary.c (streaming compression against a 1 GB dictionary). */
#define main zstd_upstream_largedict_unused_main
#include "largeDictionary.c"
#undef main

int harvest_large_dictionary(void)
{
    const char* argv[] = { "largeDictionary" };
    return zstd_upstream_largedict_unused_main(1, argv);
}
