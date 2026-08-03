/* Glue: wrap upstream libsodium test driver scalarmult6.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/scalarmult6.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream scalarmult6.c is included verbatim. */
#define main  harvest_scalarmult6_
#define xmain harvest_scalarmult6__xmain
#include "scalarmult6.c"
#undef xmain
#undef main
