/* Glue: wrap upstream libsodium test driver box_easy.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/box_easy.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream box_easy.c is included verbatim. */
#define main  harvest_box_easy_
#define xmain harvest_box_easy__xmain
#include "box_easy.c"
#undef xmain
#undef main
