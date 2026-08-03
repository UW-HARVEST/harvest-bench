/* Glue: wrap upstream libsodium test driver secretstream_xchacha20poly1305.c as a callable entry point.
 * cmptest.h renames the driver's own main() to xmain() and supplies a harness
 * main() that runs xmain(), captures stdout, and byte-compares it against
 * $TEST_SRCDIR/secretstream_xchacha20poly1305.exp (return 0 on exact match). Both xmain and that harness
 * main are file-scope symbols, so with 104 drivers linked together they would
 * collide -- rename both uniquely per TU. Upstream secretstream_xchacha20poly1305.c is included verbatim. */
#define main  harvest_secretstream_xchacha20poly1305_
#define xmain harvest_secretstream_xchacha20poly1305__xmain
#include "secretstream_xchacha20poly1305.c"
#undef xmain
#undef main
