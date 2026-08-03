/* Glue: wrap upstream libsodium test driver aead_aegis128l.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/aead_aegis128l.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream aead_aegis128l.c is included verbatim. */
#define main  harvest_aead_aegis128l_
#define xmain harvest_aead_aegis128l__xmain
#include "aead_aegis128l.c"
#undef xmain
#undef main
