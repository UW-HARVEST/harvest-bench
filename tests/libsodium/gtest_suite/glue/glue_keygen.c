/* Glue: wrap upstream libsodium test driver keygen.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/keygen.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream keygen.c is included verbatim. */
#define main  harvest_keygen_
#define xmain harvest_keygen__xmain
#include "keygen.c"
#undef xmain
#undef main
