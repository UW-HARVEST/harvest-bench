// Data-driven tests over the upstream jansson data suites
// (upstream/{valid,invalid,invalid-unicode,encoding-flags}). Upstream drives
// these with its `run` shell script + the json_process helper; here each case
// directory becomes one GoogleTest, and the upstream json_process logic is
// reused verbatim (see glue/glue_json_process.c) to parse the case's `input`
// (with any `env` config) and compare against the expected `output` (valid)
// or `error` (invalid) file.
//
// Both the "normal" and "strip" variants upstream runs are exercised. Cases
// carrying a skip_unless_dtoa / skip_if_dtoa marker are gated on the library
// build's dtoa support (this corpus builds with DTOA_ENABLED, so *_unless_
// runs and *_if_ skips).
//
// The suite root is baked in at configure time via JANSSON_GTEST_DATA_DIR.

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

extern "C" {
int harvest_json_process(const char *test_dir, int strip);
}

namespace {

namespace fs = std::filesystem;

// This corpus's library is built with dtoa enabled (jansson_private_config.h:
// DTOA_ENABLED 1). Kept as a named constant so the gating logic reads clearly.
constexpr bool kDtoaEnabled = true;

std::string DataDir() { return JANSSON_GTEST_DATA_DIR; }

std::string ParamName(const std::string& stem) {
  std::string out;
  for (char c : stem) {
    out += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
  }
  return out.empty() ? "case" : out;
}

// (suite subdir, case name) pairs, discovered at static-init time.
std::vector<std::pair<std::string, std::string>> CasesIn(const std::string& suite) {
  std::vector<std::pair<std::string, std::string>> cases;
  const fs::path root = fs::path(DataDir()) / suite;
  if (!fs::is_directory(root)) return cases;
  for (const auto& e : fs::directory_iterator(root)) {
    if (e.is_directory() && fs::exists(e.path() / "input")) {
      cases.emplace_back(suite, e.path().filename().string());
    }
  }
  std::sort(cases.begin(), cases.end());
  return cases;
}

std::vector<std::pair<std::string, std::string>> AllCases() {
  std::vector<std::pair<std::string, std::string>> all;
  for (const char* suite :
       {"valid", "invalid", "invalid-unicode", "encoding-flags"}) {
    auto c = CasesIn(suite);
    all.insert(all.end(), c.begin(), c.end());
  }
  return all;
}

class JanssonDataSuite
    : public testing::TestWithParam<std::pair<std::string, std::string>> {};

TEST_P(JanssonDataSuite, MatchesExpected) {
  const std::string suite = GetParam().first;
  const std::string name = GetParam().second;
  const fs::path dir = fs::path(DataDir()) / suite / name;

  if (fs::exists(dir / "skip_unless_dtoa") && !kDtoaEnabled) {
    GTEST_SKIP() << "requires dtoa support";
  }
  if (fs::exists(dir / "skip_if_dtoa") && kDtoaEnabled) {
    GTEST_SKIP() << "excluded when dtoa is enabled";
  }

  const std::string path = dir.string();
  // Upstream runs both the normal and the leading/trailing-whitespace-stripped
  // variants; both must reproduce the expected output/error. The strip variant
  // works on a NUL-terminated C string, so cases whose input contains an
  // embedded NUL carry a `nostrip` marker telling the runner to skip it (else
  // the input is silently truncated at the NUL).
  EXPECT_EQ(harvest_json_process(path.c_str(), 0), 0) << "normal variant";
  if (!fs::exists(dir / "nostrip")) {
    EXPECT_EQ(harvest_json_process(path.c_str(), 1), 0) << "strip variant";
  }
}

INSTANTIATE_TEST_SUITE_P(
    All, JanssonDataSuite, testing::ValuesIn(AllCases()),
    [](const testing::TestParamInfo<std::pair<std::string, std::string>>& info) {
      return ParamName(info.param.first + "_" + info.param.second);
    });

}  // namespace
