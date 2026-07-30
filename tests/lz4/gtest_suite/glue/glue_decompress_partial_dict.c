/* Glue TU for decompress-partial-usingDict.c (see glue_decompress_partial.c).
 * Both upstream files define a non-static `source` array at file scope, so it
 * is renamed here to avoid a cross-TU collision. */
#define main lz4_upstream_decompress_partial_dict_main
#define source lz4_upstream_dp_dict_source
#include "decompress-partial-usingDict.c"
#undef source
#undef main

int harvest_decompress_partial_dict(void)
{
    return lz4_upstream_decompress_partial_dict_main();
}
