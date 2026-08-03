/* Glue: wrap upstream libsodium test driver auth5.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/auth5.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream auth5.c is included verbatim. */
#define main  harvest_auth5_
#define xmain harvest_auth5__xmain
#include "auth5.c"
#undef xmain
#undef main
