/* Glue: wrap upstream libsodium test driver scalarmult8.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/scalarmult8.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream scalarmult8.c is included verbatim. */
#define main  harvest_scalarmult8_
#define xmain harvest_scalarmult8__xmain
#include "scalarmult8.c"
#undef xmain
#undef main
