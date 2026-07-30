/* Glue TU for the two upstream decompress-partial tests. Both are fully
 * deterministic `int main(void)` programs; they are simply renamed so a
 * GoogleTest wrapper can invoke them and check the return code.
 *
 * The two files share no non-static symbols, but both define main(), so
 * each gets its own rename before inclusion.
 */
#define main lz4_upstream_decompress_partial_main
#include "decompress-partial.c"
#undef main

int harvest_decompress_partial(void)
{
    return lz4_upstream_decompress_partial_main();
}
