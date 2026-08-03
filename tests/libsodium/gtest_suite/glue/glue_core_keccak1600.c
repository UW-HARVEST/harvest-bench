/* Glue: wrap upstream libsodium test driver core_keccak1600.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/core_keccak1600.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream core_keccak1600.c is included verbatim. */
#define main  harvest_core_keccak1600_
#define xmain harvest_core_keccak1600__xmain
#include "core_keccak1600.c"
#undef xmain
#undef main
