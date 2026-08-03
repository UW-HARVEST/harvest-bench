/* Glue: wrap upstream libsodium test driver kdf.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/kdf.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream kdf.c is included verbatim. */
#define main  harvest_kdf_
#define xmain harvest_kdf__xmain
#include "kdf.c"
#undef xmain
#undef main
