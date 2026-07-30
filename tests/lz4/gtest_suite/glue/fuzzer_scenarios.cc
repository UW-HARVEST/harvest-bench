// Scenario table and banner parsing for lz4's FUZ_unitTests.
//
// Kept out of glue_fuzzer.c because that translation unit must be C (it
// includes upstream's fuzzer.c verbatim). Everything here is ordinary C++
// exposed through a C interface; nothing may throw across it.

#include <array>
#include <cstddef>
#include <string_view>

#include "scenario_record.h"

namespace {

// Scenario banners emitted by FUZ_unitTests, in the order upstream prints
// them, captured from a run against the reference C library. Matching is by
// prefix against the next expected entry, which tolerates the heterogeneous
// banner styles upstream uses (some end in ":", some in " : ", some in "\n").
//
// The entries are string literals, so `.data()` is NUL-terminated and safe to
// hand back across the C interface below.
//
// Note: eight further scenarios inside FUZ_unitTests are delimited only by
// source comments (ring-buffer and ext-dict variants between the "HC
// dictionary compression test" and "LZ4_compress_HC_destSize" banners) and
// print nothing, so they are covered by the enclosing entry rather than
// reported separately.
constexpr std::array<std::string_view, 17> kScenarios{{
    "Overflow tests",
    "LZ4_decompress_safe() with empty input",
    "LZ4_decompress_safe() with one byte input",
    "LZ4_decompress_safe() with shortcut edge case",
    "LZ4_compress_default() with NULL input",
    "LZ4_compress_default() with both NULL input and output",
    "in-place compression using LZ4_compress_default()",
    "in-place decompression, limit case",
    "LZ4_initStream with multiple valid alignments",
    "saveDict (right after init)",
    "LZ4_initStreamHC with multiple valid alignments",
    "Basic HC allocation",
    "Simple HC round-trip",
    "saveDictHC test #926",
    "Long sequence HC_destSize test",
    "HC dictionary compression test",
    "LZ4_compress_HC_destSize",
}};

std::size_t g_next = 0;  // index of the next banner we expect

}  // namespace

// Called by the print hook in glue_fuzzer.c for every line upstream emits.
extern "C" void harvest_fuzzer_on_banner(const char* text) noexcept {
  if (text == nullptr || g_next >= kScenarios.size()) return;
  const std::string_view expected = kScenarios[g_next];
  if (std::string_view{text}.substr(0, expected.size()) == expected)
    sr_scenario_start(static_cast<int>(g_next++));
}

extern "C" void harvest_fuzzer_reset_banners(void) noexcept { g_next = 0; }

extern "C" int harvest_fuzzer_unit_scenario_count(void) noexcept {
  return static_cast<int>(kScenarios.size());
}

extern "C" const char* harvest_fuzzer_unit_scenario_name(int i) noexcept {
  if (i < 0 || static_cast<std::size_t>(i) >= kScenarios.size()) return "";
  return kScenarios[static_cast<std::size_t>(i)].data();
}
