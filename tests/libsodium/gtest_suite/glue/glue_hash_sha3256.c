/* Glue: wrap upstream libsodium test driver hash_sha3256.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/hash_sha3256.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream hash_sha3256.c is included verbatim. */
#define main  harvest_hash_sha3256_
#define xmain harvest_hash_sha3256__xmain
#include "hash_sha3256.c"
#undef xmain
#undef main
