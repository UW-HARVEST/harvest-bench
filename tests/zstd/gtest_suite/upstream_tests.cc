// GoogleTest wrappers around the upstream zstd v1.5.7 test programs in
// upstream/ (see upstream/README.md for provenance and exclusions).
//
// Every entry point is deterministic: the unit-test sections use constant
// seed 0 exactly as upstream's own main() does ("constant seed for
// predictability"), fuzz loops take a pinned seed with the wall-clock cutoff
// disabled, and the suite is compiled without ZSTD_MULTITHREAD (matching the
// single-threaded library build), which compiles the MT-only sections out.
// The harness runs each test in its own process, so upstream's exit-on-error
// style maps cleanly onto per-test failures.

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

extern "C" {
#include "scenario_record.h"

int harvest_fuzzer_basic_recorded(void);
int harvest_fuzzer_scenario_count(void);
int harvest_fuzzer_scenario_id(int i);
const char *harvest_fuzzer_scenario_desc(int i);
int harvest_zstream_basic_recorded(int bigTests);
int harvest_zstream_scenario_count(void);
int harvest_zstream_scenario_id(int i);
const char *harvest_zstream_scenario_desc(int i);
int harvest_fuzzer_long(void);
int harvest_fuzzer_fuzz(unsigned seed, unsigned nbTests, int bigTests);
int harvest_zstream_fuzz_old(unsigned seed, unsigned nbTests, int bigTests);
int harvest_zstream_fuzz_new(unsigned seed, int nbTests, int bigTests);
int harvest_longmatch(void);
int harvest_invalid_dictionaries(void);
int harvest_bigdict(void);
int harvest_large_dictionary(void);
}

namespace {

constexpr unsigned kSeed = 335;
constexpr int kBigTests = 1;

// Turns an upstream scenario description into a gtest test name: only
// alphanumerics survive, and upstream's own testNb is zero-padded so the cases
// list in the order upstream runs them.
std::string ScenarioLabel(int id, std::string_view desc) {
  constexpr std::size_t kMaxLabelChars = 60;
  std::string label;
  label.reserve(std::min(desc.size(), kMaxLabelChars));
  std::transform(desc.begin(), desc.end(), std::back_inserter(label),
                 [](unsigned char c) {
                   return std::isalnum(c) ? static_cast<char>(c) : '_';
                 });
  if (label.size() > kMaxLabelChars) label.resize(kMaxLabelChars);
  while (!label.empty() && label.back() == '_') label.pop_back();
  if (label.empty()) label = "scenario";

  std::string prefix = std::to_string(id);
  if (prefix.size() < 3) prefix.insert(0, 3 - prefix.size(), '0');
  return prefix + '_' + label;
}

// 0..n-1, the ordinals used to index the scenario tables.
std::vector<int> Ordinals(int n) {
  std::vector<int> v(static_cast<std::size_t>(std::max(n, 0)));
  std::iota(v.begin(), v.end(), 0);
  return v;
}

// Executing an upstream monolith is split out as its own case so that cost
// belongs to one named test instead of landing on whichever Scenario case the
// harness happens to schedule first. GoogleTest lists every non-parameterized
// suite before the parameterized ones, so these run first and leave a warm
// record; each Scenario case then only reads it (~3 ms), which is what makes
// the per-test budgets in budgets.json meaningful. They assert only that a
// record was produced - the scenario verdicts carry the actual outcome, so a
// crashed or hung monolith is not counted twice.
TEST(UpstreamUnitTestRecording, FuzzerBasic) {
  EXPECT_EQ(harvest_fuzzer_basic_recorded(), 0)
      << "could not record a run of basicUnitTests";
}

TEST(UpstreamUnitTestRecording, ZstreamBasic) {
  EXPECT_EQ(harvest_zstream_basic_recorded(kBigTests), 0)
      << "could not record a run of zstreamtest basicUnitTests";
}

// fuzzer.c: single-shot, block-level, cctx-reuse, dictionary and advanced-
// parameter unit tests. Upstream runs this with constant seed 0.
//
// Its ~205 scenarios are reported individually even though the upstream
// function executes only once per library: the glue records scenario
// boundaries while a forked child runs it, and the cases below read that
// record (see glue/scenario_record.h). A scenario upstream never reached,
// because an earlier one aborted the run, is a failure rather than a skip —
// skips leave the harness's denominator, which would let an early crash score
// better than a late one.
class FuzzerBasic : public testing::TestWithParam<int> {};

TEST_P(FuzzerBasic, Scenario) {
  ASSERT_EQ(harvest_fuzzer_basic_recorded(), 0)
      << "could not record a run of basicUnitTests";
  const int id = harvest_fuzzer_scenario_id(GetParam());
  const std::string_view desc = harvest_fuzzer_scenario_desc(GetParam());
  switch (sr_verdict(id)) {
    case SR_PASSED: SUCCEED(); break;
    case SR_FAILED: FAIL() << "upstream aborted in scenario " << id << ": " << desc; break;
    case SR_NOT_REACHED:
      FAIL() << "not reached (an earlier scenario aborted the run): " << desc;
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    UpstreamFuzzer, FuzzerBasic,
    testing::ValuesIn(Ordinals(harvest_fuzzer_scenario_count())),
    [](const testing::TestParamInfo<int>& i) {
      return ScenarioLabel(harvest_fuzzer_scenario_id(i.param),
                           harvest_fuzzer_scenario_desc(i.param));
    });

// fuzzer.c --long-tests section (larger data, more expensive scenarios).
TEST(UpstreamFuzzer, LongUnitTests) {
  EXPECT_EQ(harvest_fuzzer_long(), 0);
}

// Fixed-seed randomized loop over the single-shot API.
TEST(UpstreamFuzzer, FuzzLoopSeed335) {
  EXPECT_EQ(harvest_fuzzer_fuzz(kSeed, 64, kBigTests), 0);
}

// zstreamtest.c: streaming API unit tests (constant seed 0 upstream), split
// per scenario the same way as FuzzerBasic above.
class ZstreamBasic : public testing::TestWithParam<int> {};

TEST_P(ZstreamBasic, Scenario) {
  ASSERT_EQ(harvest_zstream_basic_recorded(kBigTests), 0)
      << "could not record a run of zstreamtest basicUnitTests";
  const int id = harvest_zstream_scenario_id(GetParam());
  const std::string_view desc = harvest_zstream_scenario_desc(GetParam());
  switch (sr_verdict(id)) {
    case SR_PASSED: SUCCEED(); break;
    case SR_FAILED: FAIL() << "upstream aborted in scenario " << id << ": " << desc; break;
    case SR_NOT_REACHED:
      FAIL() << "not reached (an earlier scenario aborted the run): " << desc;
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    UpstreamZstream, ZstreamBasic,
    testing::ValuesIn(Ordinals(harvest_zstream_scenario_count())),
    [](const testing::TestParamInfo<int>& i) {
      return ScenarioLabel(harvest_zstream_scenario_id(i.param),
                           harvest_zstream_scenario_desc(i.param));
    });

// Fixed-seed streaming fuzz loops, old (ZSTD_compressStream) and new
// (ZSTD_compressStream2 / advanced parameters) API surfaces.
TEST(UpstreamZstream, FuzzLoopOldApiSeed335) {
  EXPECT_EQ(harvest_zstream_fuzz_old(kSeed, 64, kBigTests), 0);
}

TEST(UpstreamZstream, FuzzLoopNewApiSeed335) {
  EXPECT_EQ(harvest_zstream_fuzz_new(kSeed, 64, kBigTests), 0);
}

// longmatch.c: long-distance match handling through the streaming API.
TEST(UpstreamLongMatch, Basic) {
  EXPECT_EQ(harvest_longmatch(), 0);
}

// invalidDictionaries.c: malformed dictionaries must be rejected.
TEST(UpstreamInvalidDictionaries, Rejected) {
  EXPECT_EQ(harvest_invalid_dictionaries(), 0);
}

// bigdict.c / largeDictionary.c: >=1 GiB dictionary windows; the heaviest
// tests in the suite (memory and wall-clock).
TEST(UpstreamBigDict, RoundTrip) {
  EXPECT_EQ(harvest_bigdict(), 0);
}

TEST(UpstreamLargeDictionary, StreamRoundTrip) {
  EXPECT_EQ(harvest_large_dictionary(), 0);
}

}  // namespace
