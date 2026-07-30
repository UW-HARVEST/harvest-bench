/* Glue TU: wraps upstream jansson test_dump_callback.c as a GoogleTest entry point.
 * main() lives in api/util.h and is renamed here; the upstream file is
 * included verbatim. */
#define main jansson_test_dump_callback_main
#include "api/test_dump_callback.c"
#undef main

int harvest_test_dump_callback(void) { return jansson_test_dump_callback_main(); }
