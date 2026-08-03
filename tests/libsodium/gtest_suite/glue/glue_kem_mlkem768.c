/* Glue: wrap upstream libsodium test driver kem_mlkem768.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/kem_mlkem768.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream kem_mlkem768.c is included verbatim. */
#define main  harvest_kem_mlkem768_
#define xmain harvest_kem_mlkem768__xmain
#include "kem_mlkem768.c"
#undef xmain
#undef main
