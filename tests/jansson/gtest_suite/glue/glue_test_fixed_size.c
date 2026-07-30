/* Glue TU: wraps upstream jansson test_fixed_size.c as a GoogleTest entry point.
 * main() lives in api/util.h and is renamed here; the upstream file is
 * included verbatim. */
#define main jansson_test_fixed_size_main
#include "api/test_fixed_size.c"
#undef main

int harvest_test_fixed_size(void) { return jansson_test_fixed_size_main(); }
