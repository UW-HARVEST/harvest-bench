/* Glue TU: wraps upstream jansson test_chaos_realloc.c as a GoogleTest entry point.
 * main() lives in api/util.h and is renamed here; the upstream file is
 * included verbatim. */
/* Rename file-scope symbols that also exist in test_chaos.c to avoid a
 * cross-TU link collision (both files are linked into one binary). */
#define chaos_malloc        chaos_realloc_variant_malloc
#define chaos_realloc       chaos_realloc_variant_realloc
#define chaos_free          chaos_realloc_variant_free
#define test_unpack         chaos_realloc_variant_test_unpack
#define dump_chaos_callback chaos_realloc_variant_dump_callback
#define main jansson_test_chaos_realloc_main
#include "api/test_chaos_realloc.c"
#undef main

int harvest_test_chaos_realloc(void) { return jansson_test_chaos_realloc_main(); }
