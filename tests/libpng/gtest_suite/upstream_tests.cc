// GoogleTest wrappers around the upstream libpng correctness validator
// (upstream/pngvalid.c, see ../README.md for provenance). pngvalid is libpng's
// own ~12k-line validator: it self-generates PNG images across bit depths,
// color types, and interlacing, writes and reads them back through the public
// API, and checks the round-tripped pixels against computed expectations with
// tight numeric tolerances (gamma, transforms). It uses only the library's
// PUBLIC C ABI, so the same suite runs against the C reference or a translated
// Rust cdylib.
//
// glue/glue_pngvalid.c includes pngvalid.c verbatim (main renamed) and exposes
// one entry point per test group. Each group runs in its own process (see
// manifest.json) because pngvalid keeps global state across a run.

#include <gtest/gtest.h>

extern "C" {
int harvest_pngvalid_standard(void);
int harvest_pngvalid_size(void);
int harvest_pngvalid_transform(void);
int harvest_pngvalid_gamma(void);
}

TEST(Libpng, PngvalidStandard)  { EXPECT_EQ(harvest_pngvalid_standard(), 0); }
TEST(Libpng, PngvalidSize)      { EXPECT_EQ(harvest_pngvalid_size(), 0); }
TEST(Libpng, PngvalidTransform) { EXPECT_EQ(harvest_pngvalid_transform(), 0); }
TEST(Libpng, PngvalidGamma)     { EXPECT_EQ(harvest_pngvalid_gamma(), 0); }
