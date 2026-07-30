/* Glue TU for invalidDictionaries.c (malformed dictionaries must be rejected). */
#define main zstd_upstream_invaliddicts_unused_main
#include "invalidDictionaries.c"
#undef main

int harvest_invalid_dictionaries(void)
{
    const char* argv[] = { "invalidDictionaries" };
    return zstd_upstream_invaliddicts_unused_main(1, argv);
}
