/* Glue: wrap upstream libsodium test driver kx.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/kx.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream kx.c is included verbatim. */
#define main  harvest_kx_
#define xmain harvest_kx__xmain
#include "kx.c"
#undef xmain
#undef main
