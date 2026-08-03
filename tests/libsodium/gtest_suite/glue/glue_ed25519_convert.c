/* Glue: wrap upstream libsodium test driver ed25519_convert.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/ed25519_convert.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream ed25519_convert.c is included verbatim. */
#define main  harvest_ed25519_convert_
#define xmain harvest_ed25519_convert__xmain
#include "ed25519_convert.c"
#undef xmain
#undef main
