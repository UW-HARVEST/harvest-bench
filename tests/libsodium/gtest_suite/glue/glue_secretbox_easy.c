/* Glue: wrap upstream libsodium test driver secretbox_easy.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/secretbox_easy.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream secretbox_easy.c is included verbatim. */
#define main  harvest_secretbox_easy_
#define xmain harvest_secretbox_easy__xmain
#include "secretbox_easy.c"
#undef xmain
#undef main
