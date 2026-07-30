/* Glue TU: wraps upstream jansson test_sprintf.c as a GoogleTest entry point.
 * main() lives in api/util.h and is renamed here; the upstream file is
 * included verbatim. */
#define main jansson_test_sprintf_main
#include "api/test_sprintf.c"
#undef main

int harvest_test_sprintf(void) { return jansson_test_sprintf_main(); }
