/* Glue TU: wraps upstream jansson test_version.c as a GoogleTest entry point.
 * main() lives in api/util.h and is renamed here; the upstream file is
 * included verbatim. */
#define main jansson_test_version_main
#include "api/test_version.c"
#undef main

int harvest_test_version(void) { return jansson_test_version_main(); }
