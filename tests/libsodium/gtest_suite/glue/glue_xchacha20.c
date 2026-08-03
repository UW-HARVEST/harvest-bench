/* Glue: wrap upstream libsodium test driver xchacha20.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/xchacha20.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream xchacha20.c is included verbatim. */
#define main  harvest_xchacha20_
#define xmain harvest_xchacha20__xmain
#include "xchacha20.c"
#undef xmain
#undef main
