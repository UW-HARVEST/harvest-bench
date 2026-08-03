/* Glue: wrap upstream libsodium test driver ipcrypt.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/ipcrypt.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream ipcrypt.c is included verbatim. */
#define main  harvest_ipcrypt_
#define xmain harvest_ipcrypt__xmain
#include "ipcrypt.c"
#undef xmain
#undef main
