/* Glue: wrap upstream libsodium test driver kdf_hkdf3.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/kdf_hkdf3.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream kdf_hkdf3.c is included verbatim. */
#define main  harvest_kdf_hkdf3_
#define xmain harvest_kdf_hkdf3__xmain
#include "kdf_hkdf3.c"
#undef xmain
#undef main
