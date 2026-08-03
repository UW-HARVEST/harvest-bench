/* Glue: wrap upstream libsodium test driver secretbox8.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/secretbox8.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream secretbox8.c is included verbatim. */
#define main  harvest_secretbox8_
#define xmain harvest_secretbox8__xmain
#include "secretbox8.c"
#undef xmain
#undef main
