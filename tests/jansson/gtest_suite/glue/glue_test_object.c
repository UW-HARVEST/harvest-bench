/* Glue TU: wraps upstream jansson test_object.c as a GoogleTest entry point.
 * main() lives in api/util.h and is renamed here; the upstream file is
 * included verbatim. */
#define main jansson_test_object_main
#include "api/test_object.c"
#undef main

int harvest_test_object(void) { return jansson_test_object_main(); }
