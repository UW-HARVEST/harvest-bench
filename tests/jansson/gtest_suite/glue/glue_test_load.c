/* Glue TU: wraps upstream jansson test_load.c as a GoogleTest entry point.
 * main() lives in api/util.h and is renamed here; the upstream file is
 * included verbatim. */
#define main jansson_test_load_main
#include "api/test_load.c"
#undef main

int harvest_test_load(void) { return jansson_test_load_main(); }
