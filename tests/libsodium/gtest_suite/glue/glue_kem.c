/* Glue: wrap upstream libsodium test driver kem.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/kem.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream kem.c is included verbatim. */
#define main  harvest_kem_
#define xmain harvest_kem__xmain
#include "kem.c"
#undef xmain
#undef main
