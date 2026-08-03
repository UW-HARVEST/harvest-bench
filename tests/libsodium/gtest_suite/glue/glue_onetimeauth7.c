/* Glue: wrap upstream libsodium test driver onetimeauth7.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/onetimeauth7.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream onetimeauth7.c is included verbatim. */
#define main  harvest_onetimeauth7_
#define xmain harvest_onetimeauth7__xmain
#include "onetimeauth7.c"
#undef xmain
#undef main
