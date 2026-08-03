/* Glue: wrap upstream libsodium test driver sign2.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/sign2.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream sign2.c is included verbatim. */
#define main  harvest_sign2_
#define xmain harvest_sign2__xmain
#include "sign2.c"
#undef xmain
#undef main
