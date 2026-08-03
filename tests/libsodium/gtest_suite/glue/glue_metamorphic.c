/* Glue: wrap upstream libsodium test driver metamorphic.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/metamorphic.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream metamorphic.c is included verbatim. */
#define main  harvest_metamorphic_
#define xmain harvest_metamorphic__xmain
#include "metamorphic.c"
#undef xmain
#undef main
