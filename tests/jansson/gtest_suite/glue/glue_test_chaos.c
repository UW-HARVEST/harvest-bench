/* Glue TU: wraps upstream jansson test_chaos.c as a GoogleTest entry point.
 * main() lives in api/util.h and is renamed here; the upstream file is
 * included verbatim. */
#define main jansson_test_chaos_main
#include "api/test_chaos.c"
#undef main

int harvest_test_chaos(void) { return jansson_test_chaos_main(); }
