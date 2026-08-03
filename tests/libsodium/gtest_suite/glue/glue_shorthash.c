/* Glue: wrap upstream libsodium test driver shorthash.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/shorthash.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream shorthash.c is included verbatim. */
#define main  harvest_shorthash_
#define xmain harvest_shorthash__xmain
#include "shorthash.c"
#undef xmain
#undef main
