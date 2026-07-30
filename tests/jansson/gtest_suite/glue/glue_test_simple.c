/* Glue TU: wraps upstream jansson test_simple.c as a GoogleTest entry point.
 * main() lives in api/util.h and is renamed here; the upstream file is
 * included verbatim. */
#define main jansson_test_simple_main
#include "api/test_simple.c"
#undef main

int harvest_test_simple(void) { return jansson_test_simple_main(); }
