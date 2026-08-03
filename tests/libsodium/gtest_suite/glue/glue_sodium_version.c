/* Glue: wrap upstream libsodium test driver sodium_version.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/sodium_version.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream sodium_version.c is included verbatim. */
#define main  harvest_sodium_version_
#define xmain harvest_sodium_version__xmain
#include "sodium_version.c"
#undef xmain
#undef main
