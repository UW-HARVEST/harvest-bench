// GoogleTest wrappers around the upstream lz4 v1.10.0 test programs in
// upstream/ (see upstream/README.md for provenance and what was excluded).
//
// Each wrapped program asserts internally and exits nonzero on failure; the
// harness runs every test case in its own process, so a wrapped failure or
// crash is attributed to exactly one test here. All entry points are
// deterministic: seeds are fixed, wall-clock cutoffs disabled, compression
// levels explicit. Cycle counts are chosen to keep the full suite's runtime
// to a few seconds.

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

extern "C" {
#include "scenario_record.h"
#include "lz4hc.h" /* LZ4HC_CLEVEL_DEFAULT / LZ4HC_CLEVEL_OPT_MIN */

int harvest_fuzzer_unit_tests_recorded(int compressionLevel, const char *tag);
int harvest_fuzzer_unit_scenario_count(void);
const char *harvest_fuzzer_unit_scenario_name(int i);
int harvest_fuzzer_fuzz(unsigned seed, unsigned nbCycles, int compressibilityPct);
int harvest_frametest_unit_tests(unsigned seed, int compressibilityPct);
int harvest_frametest_fuzz(unsigned seed, unsigned nbTests, int compressibilityPct);
int harvest_roundtrip(int clevel, size_t size, unsigned seed);
int harvest_decompress_partial(void);
int harvest_decompress_partial_dict(void);
}

namespace {

// Upstream defaults: fuzzer.c -P 60, frametest.c -P 50.
constexpr int kFuzzerCompressibility = 60;
constexpr int kFrametestCompressibility = 50;
constexpr unsigned kSeed = 335;

// Deterministic unit-test section of fuzzer.c (block + HC + streaming APIs),
// which upstream runs at both of these HC levels when invoked with no seed.
//
// Each of its 17 scenarios is reported as its own test case even though the
// upstream function is executed only once per level: the glue records scenario
// boundaries while a forked child runs it, and the cases below read that
// record (see glue/scenario_record.h). A scenario that upstream never reached,
// because an earlier one aborted the run, is a failure rather than a skip —
// skips leave the harness's denominator, which would let an early crash score
// better than a late one.
struct UnitScenario {
  int index;
  int level;
  // Names the record file for this level's run. Always a literal, so .data()
  // is NUL-terminated where it crosses back into C.
  std::string_view tag;
};

// Executing the upstream function is split out as its own case so that cost
// belongs to one named test instead of landing on whichever Scenario case the
// harness happens to schedule first. GoogleTest lists every non-parameterized
// suite before the parameterized ones, so these run first and leave a warm
// record; each Scenario case then only reads it (~3 ms), which is what makes
// the per-test budgets in budgets.json meaningful. They assert only that a
// record was produced - the scenario verdicts carry the actual outcome, so a
// crashed or hung monolith is not counted twice.
TEST(UpstreamFuzzerUnitTests, RecordHcClevelDefault) {
  EXPECT_EQ(harvest_fuzzer_unit_tests_recorded(LZ4HC_CLEVEL_DEFAULT,
                                               "unit_hc_default"),
            0)
      << "could not record a run of FUZ_unitTests";
}

TEST(UpstreamFuzzerUnitTests, RecordHcClevelOptMin) {
  EXPECT_EQ(harvest_fuzzer_unit_tests_recorded(LZ4HC_CLEVEL_OPT_MIN,
                                               "unit_hc_optmin"),
            0)
      << "could not record a run of FUZ_unitTests";
}

void ReportScenario(const UnitScenario& s) {
  ASSERT_EQ(harvest_fuzzer_unit_tests_recorded(s.level, s.tag.data()), 0)
      << "could not record a run of FUZ_unitTests";
  const std::string_view name = harvest_fuzzer_unit_scenario_name(s.index);
  switch (sr_verdict(s.index)) {
    case SR_PASSED:
      SUCCEED();
      break;
    case SR_FAILED:
      FAIL() << "upstream aborted in scenario: " << name;
      break;
    case SR_NOT_REACHED:
      FAIL() << "not reached (an earlier scenario aborted the run): " << name;
      break;
  }
}

std::vector<UnitScenario> ScenariosFor(int level, std::string_view tag) {
  std::vector<UnitScenario> out;
  for (int i = 0; i < harvest_fuzzer_unit_scenario_count(); ++i)
    out.push_back(UnitScenario{i, level, tag});
  return out;
}

// Turns an upstream scenario description into a gtest test name: only
// alphanumerics survive, and the index is zero-padded so the cases list in the
// order upstream runs them.
std::string ScenarioLabel(int index) {
  const std::string_view desc = harvest_fuzzer_unit_scenario_name(index);
  std::string label;
  label.reserve(desc.size());
  std::transform(desc.begin(), desc.end(), std::back_inserter(label),
                 [](unsigned char c) {
                   return std::isalnum(c) ? static_cast<char>(c) : '_';
                 });
  while (!label.empty() && label.back() == '_') label.pop_back();
  if (label.empty()) label = "scenario";

  std::string prefix = std::to_string(index);
  if (prefix.size() < 2) prefix.insert(0, 2 - prefix.size(), '0');
  return prefix + '_' + label;
}

class UnitTestsHcClevelDefault : public testing::TestWithParam<UnitScenario> {};
TEST_P(UnitTestsHcClevelDefault, Scenario) { ReportScenario(GetParam()); }
INSTANTIATE_TEST_SUITE_P(
    UpstreamFuzzer, UnitTestsHcClevelDefault,
    testing::ValuesIn(ScenariosFor(LZ4HC_CLEVEL_DEFAULT, "unit_hc_default")),
    [](const testing::TestParamInfo<UnitScenario>& i) {
      return ScenarioLabel(i.param.index);
    });

class UnitTestsHcClevelOptMin : public testing::TestWithParam<UnitScenario> {};
TEST_P(UnitTestsHcClevelOptMin, Scenario) { ReportScenario(GetParam()); }
INSTANTIATE_TEST_SUITE_P(
    UpstreamFuzzer, UnitTestsHcClevelOptMin,
    testing::ValuesIn(ScenariosFor(LZ4HC_CLEVEL_OPT_MIN, "unit_hc_optmin")),
    [](const testing::TestParamInfo<UnitScenario>& i) {
      return ScenarioLabel(i.param.index);
    });

// Fixed-seed randomized differential loop over the block API. Upstream CI
// runs this time-boxed with a time-derived seed; here the seed is pinned and
// the workload is a fixed cycle count.
TEST(UpstreamFuzzer, FuzzLoopSeed42) {
  EXPECT_EQ(harvest_fuzzer_fuzz(kSeed, 256, kFuzzerCompressibility), 0);
}

// Deterministic unit-test section of frametest.c (LZ4F begin/update/end
// cascades, dictionaries, bug regressions). Takes the seed explicitly.
TEST(UpstreamFrametest, UnitTestsSeed42) {
  EXPECT_EQ(harvest_frametest_unit_tests(kSeed, kFrametestCompressibility), 0);
}

TEST(UpstreamFrametest, FuzzLoopSeed42) {
  EXPECT_EQ(harvest_frametest_fuzz(kSeed, 96, kFrametestCompressibility), 0);
}

// Round trip over every distinct compression level in the upstream random
// range (MIN_CLEVEL = -5 .. LZ4HC_CLEVEL_MAX = 12) with a fixed
// pseudo-random buffer. Negative levels are LZ4_compress_fast acceleration
// values, 1 is the plain fast path, and 2..12 are LZ4_compress_HC (10..12
// engage the optimal parser). Level 0 is deliberately absent: upstream
// treats it as "derive one of the other levels from a content hash", not as
// an independent level (pinned per the no-hidden-randomness rule).
class UpstreamRoundTrip : public testing::TestWithParam<int> {};

TEST_P(UpstreamRoundTrip, RoundTrip256KiB) {
  EXPECT_EQ(harvest_roundtrip(GetParam(), 256 * 1024, kSeed), 0);
}

INSTANTIATE_TEST_SUITE_P(
    AllLevels, UpstreamRoundTrip,
    testing::Values(-5, -4, -3, -2, -1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
                    12),
    [](const testing::TestParamInfo<int>& info) {
      const int lv = info.param;
      if (lv < 0) return "FastAccel" + std::to_string(-lv);
      if (lv < LZ4HC_CLEVEL_MIN) return "Fast" + std::to_string(lv);
      return "Hc" + std::to_string(lv);
    });

TEST(UpstreamDecompressPartial, Basic) {
  EXPECT_EQ(harvest_decompress_partial(), 0);
}

TEST(UpstreamDecompressPartial, UsingDict) {
  EXPECT_EQ(harvest_decompress_partial_dict(), 0);
}

}  // namespace
