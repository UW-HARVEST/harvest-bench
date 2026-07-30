/* Glue TU: wraps upstream jansson test_number.c as a GoogleTest entry point.
 * main() lives in api/util.h and is renamed here; the upstream file is
 * included verbatim. */
#define main jansson_test_number_main
#include "api/test_number.c"
#undef main

int harvest_test_number(void) { return jansson_test_number_main(); }
