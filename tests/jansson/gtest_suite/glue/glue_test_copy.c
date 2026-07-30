/* Glue TU: wraps upstream jansson test_copy.c as a GoogleTest entry point.
 * main() lives in api/util.h and is renamed here; the upstream file is
 * included verbatim. */
#define main jansson_test_copy_main
#include "api/test_copy.c"
#undef main

int harvest_test_copy(void) { return jansson_test_copy_main(); }
