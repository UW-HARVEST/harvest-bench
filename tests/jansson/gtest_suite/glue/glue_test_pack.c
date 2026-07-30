/* Glue TU: wraps upstream jansson test_pack.c as a GoogleTest entry point.
 * main() lives in api/util.h and is renamed here; the upstream file is
 * included verbatim. */
#define main jansson_test_pack_main
#include "api/test_pack.c"
#undef main

int harvest_test_pack(void) { return jansson_test_pack_main(); }
