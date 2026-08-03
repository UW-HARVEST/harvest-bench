/* Glue: wrap upstream libsodium test driver secretbox_easy2.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/secretbox_easy2.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream secretbox_easy2.c is included verbatim. */
#define main  harvest_secretbox_easy2_
#define xmain harvest_secretbox_easy2__xmain
#include "secretbox_easy2.c"
#undef xmain
#undef main
