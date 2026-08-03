/* Glue: wrap upstream libsodium test driver pwhash_scrypt.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/pwhash_scrypt.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream pwhash_scrypt.c is included verbatim. */
#define main  harvest_pwhash_scrypt_
#define xmain harvest_pwhash_scrypt__xmain
#include "pwhash_scrypt.c"
#undef xmain
#undef main
