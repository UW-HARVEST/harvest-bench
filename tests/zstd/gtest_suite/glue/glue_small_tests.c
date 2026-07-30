/* Glue TU for the small standalone upstream test programs. Each has its own
 * main(); they are included with main renamed and driven with minimal argv.
 * They are in separate include blocks below only when their file-scope
 * symbols do not collide; colliding programs get their own glue TU. */

/* longmatch.c: repeated long-distance matches through the streaming API */
#define main zstd_upstream_longmatch_unused_main
#include "longmatch.c"
#undef main

int harvest_longmatch(void)
{
    const char* argv[] = { "longmatch" };
    return zstd_upstream_longmatch_unused_main(1, argv);
}
