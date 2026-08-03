/* Glue: wrap upstream libsodium test driver onetimeauth2.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/onetimeauth2.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream onetimeauth2.c is included verbatim. */
#define main  harvest_onetimeauth2_
#define xmain harvest_onetimeauth2__xmain
#include "onetimeauth2.c"
#undef xmain
#undef main
