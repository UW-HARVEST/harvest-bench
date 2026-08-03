/* Glue: wrap upstream libsodium test driver stream.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/stream.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream stream.c is included verbatim. */
#define main  harvest_stream_
#define xmain harvest_stream__xmain
#include "stream.c"
#undef xmain
#undef main
