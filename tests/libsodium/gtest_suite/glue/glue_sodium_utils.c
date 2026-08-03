/* Glue: wrap upstream libsodium test driver sodium_utils.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/sodium_utils.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream sodium_utils.c is included verbatim. */
#define main  harvest_sodium_utils_
#define xmain harvest_sodium_utils__xmain
#include "sodium_utils.c"
#undef xmain
#undef main
