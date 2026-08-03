/* Glue: wrap upstream libsodium test driver core_ed25519_h2c.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/core_ed25519_h2c.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream core_ed25519_h2c.c is included verbatim. */
#define main  harvest_core_ed25519_h2c_
#define xmain harvest_core_ed25519_h2c__xmain
#include "core_ed25519_h2c.c"
#undef xmain
#undef main
