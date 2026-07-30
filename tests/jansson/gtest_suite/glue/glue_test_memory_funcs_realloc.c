/* Glue TU: wraps upstream jansson test_memory_funcs_realloc.c as a GoogleTest entry point.
 * main() lives in api/util.h and is renamed here; the upstream file is
 * included verbatim. */
#define main jansson_test_memory_funcs_realloc_main
#include "api/test_memory_funcs_realloc.c"
#undef main

int harvest_test_memory_funcs_realloc(void) { return jansson_test_memory_funcs_realloc_main(); }
