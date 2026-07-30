/* Glue TU: wraps upstream jansson test_dump.c as a GoogleTest entry point.
 * main() lives in api/util.h and is renamed here; the upstream file is
 * included verbatim. */
#define main jansson_test_dump_main
#include "api/test_dump.c"
#undef main

int harvest_test_dump(void) { return jansson_test_dump_main(); }
