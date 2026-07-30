/* Glue TU: wraps upstream jansson test_loadb.c as a GoogleTest entry point.
 * main() lives in api/util.h and is renamed here; the upstream file is
 * included verbatim. */
#define main jansson_test_loadb_main
#include "api/test_loadb.c"
#undef main

int harvest_test_loadb(void) { return jansson_test_loadb_main(); }
