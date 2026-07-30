/* Glue TU: wraps upstream jansson test_equal.c as a GoogleTest entry point.
 * main() lives in api/util.h and is renamed here; the upstream file is
 * included verbatim. */
#define main jansson_test_equal_main
#include "api/test_equal.c"
#undef main

int harvest_test_equal(void) { return jansson_test_equal_main(); }
