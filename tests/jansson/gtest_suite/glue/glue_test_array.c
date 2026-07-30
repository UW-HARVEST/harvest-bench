/* Glue TU: wraps upstream jansson test_array.c as a GoogleTest entry point.
 * main() lives in api/util.h and is renamed here; the upstream file is
 * included verbatim. */
#define main jansson_test_array_main
#include "api/test_array.c"
#undef main

int harvest_test_array(void) { return jansson_test_array_main(); }
