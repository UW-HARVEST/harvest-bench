/* Glue: wrap the upstream libpng correctness validator (pngvalid.c) as callable
 * entry points, one per test group. pngvalid.c is included VERBATIM with its
 * main() renamed to pngvalid_main(); PNG_FREESTANDING_TESTS makes it include
 * <png.h>/<zlib.h>/<cexcept.h> from the include path instead of upstream-tree
 * relative paths. Each harvest_pngvalid_<group>() builds an argv and calls it;
 * pngvalid returns 0 on success. pngvalid self-generates all its test images,
 * so no external files are needed. */
#define PNG_FREESTANDING_TESTS
#define main pngvalid_main
#include "pngvalid.c"
#undef main

static int run_pngvalid(int argc, char **argv) {
    return pngvalid_main(argc, argv);
}

/* --standard: baseline read/write across bit depths, color types, interlacing. */
int harvest_pngvalid_standard(void) {
    char *argv[] = {(char*)"pngvalid", (char*)"--standard", (char*)"--nogamma",
                    (char*)"--notransform", (char*)"--nosize", 0};
    return run_pngvalid(5, argv);
}
/* --size: image size handling. */
int harvest_pngvalid_size(void) {
    char *argv[] = {(char*)"pngvalid", (char*)"--nostandard", (char*)"--nogamma",
                    (char*)"--notransform", (char*)"--size", 0};
    return run_pngvalid(5, argv);
}
/* --transform: the read/write transform pipeline (expand, strip, gray<->rgb, ...). */
int harvest_pngvalid_transform(void) {
    char *argv[] = {(char*)"pngvalid", (char*)"--nostandard", (char*)"--nogamma",
                    (char*)"--transform", (char*)"--nosize", 0};
    return run_pngvalid(5, argv);
}
/* --gamma: gamma correction (the numerically tightest group). */
int harvest_pngvalid_gamma(void) {
    char *argv[] = {(char*)"pngvalid", (char*)"--nostandard", (char*)"--gamma",
                    (char*)"--notransform", (char*)"--nosize", 0};
    return run_pngvalid(5, argv);
}
