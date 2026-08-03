/* Glue: wrap upstream libsodium test driver xof_shake128.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/xof_shake128.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream xof_shake128.c is included verbatim. */
#define main  harvest_xof_shake128_
#define xmain harvest_xof_shake128__xmain
#include "xof_shake128.c"
#undef xmain
#undef main
