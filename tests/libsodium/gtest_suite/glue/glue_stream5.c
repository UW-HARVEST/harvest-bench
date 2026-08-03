/* Glue: wrap upstream libsodium test driver stream5.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/stream5.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream stream5.c is included verbatim. */
#define main  harvest_stream5_
#define xmain harvest_stream5__xmain
#include "stream5.c"
#undef xmain
#undef main
