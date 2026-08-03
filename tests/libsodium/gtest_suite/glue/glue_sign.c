/* Glue: wrap upstream libsodium test driver sign.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/sign.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream sign.c is included verbatim. */
#define main  harvest_sign_
#define xmain harvest_sign__xmain
#include "sign.c"
#undef xmain
#undef main
