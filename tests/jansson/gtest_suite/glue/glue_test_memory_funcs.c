/* Glue TU: wraps upstream jansson test_memory_funcs.c as a GoogleTest entry point.
 * main() lives in api/util.h and is renamed here; the upstream file is
 * included verbatim. */
#define main jansson_test_memory_funcs_main
#include "api/test_memory_funcs.c"
#undef main

int harvest_test_memory_funcs(void) { return jansson_test_memory_funcs_main(); }
