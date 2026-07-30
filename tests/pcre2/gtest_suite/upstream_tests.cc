// GoogleTest wrappers around the upstream pcre2test.c test-driver program,
// run against the real upstream testinput*/testoutput* data files in
// upstream/testdata/ (see upstream/README.md for provenance and exclusions).
//
// Upstream's RunTest granularity is one byte-diff per input FILE, but each
// file is a monolith of hundreds of independent pattern/subject blocks
// (blank-line separated; ~7300 blocks across the 14 ported files). To keep
// per-block attribution without re-running a whole file once per block:
//
//   - Pcre2FileRun.<Test> runs pcre2test over one input file exactly as
//     RunTest does — forked child, output streamed back through a pipe and
//     memoized in process-local memory — and asserts run health (exit
//     status) plus output STRUCTURE (same number of blocks as the expected
//     file). pcre2test never stops at a mismatch, so unless the library
//     crashes the full output always exists.
//   - Blocks/<Test>.Matches/BlockNNNN reads that in-memory record and
//     compares only its own block against the matching expected block. Input
//     lines are echoed into the output, so block boundaries in actual and
//     expected output align 1:1 (verified: all 14 files have identical
//     input/output block counts on the reference C build); each comparison
//     is anchored on the echoed first line. A block past the end of a
//     truncated (crashed) run fails as NOT_REACHED - a failure, not a skip,
//     so an early crash can never score better than a late one.
//
// Execution economics: the record lives in process memory only (no disk
// cache), so everything that shares a process shares one recording run.
// Under the HARVEST harness, gtest_suite/manifest.json batches each file's
// recording test and its block tests into one process invocation. Standard
// standalone gtest usage works unchanged; note that each separate
// single-case invocation re-runs its file's recording (~30 ms here).
//
// Blocks are not fully independent: pcre2test state (#directives, test 20's
// serialization save/load) flows through a file, so one corrupted block can
// cascade into later ones. Same accepted caveat as the zstd scenario split.

#include <gtest/gtest.h>

#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

extern "C" int harvest_pcre2test(int argc, char **argv);

namespace {

// ---------------------------------------------------------------------------
// Small helpers

std::string TestDataPath(const std::string &name) {
  return std::string(UPSTREAM_TESTDATA_DIR) + "/" + name;
}

std::vector<char> ReadFile(const std::string &path) {
  FILE *f = fopen(path.c_str(), "rb");
  if (f == nullptr) return {};
  std::vector<char> data;
  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
    data.insert(data.end(), buf, buf + n);
  }
  fclose(f);
  return data;
}

std::vector<char *> BuildArgv(const std::vector<std::string> &args) {
  std::vector<char *> argv;
  argv.push_back(const_cast<char *>("pcre2test"));
  for (const auto &a : args) argv.push_back(const_cast<char *>(a.c_str()));
  return argv;
}

// ---------------------------------------------------------------------------
// Running pcre2test in a forked child (it is single-run-per-process; see
// upstream/README.md). The child's stdout is a pipe; the output file
// argument is /dev/stdout, so both the normal outfile mode and the -error
// mode (bare printf) stream back the same way. The parent must drain the
// pipe WHILE the child runs - the output exceeds the pipe buffer.

int RunPcre2TestToPipe(const std::vector<std::string> &args,
                       std::vector<char> *out) {
  int fds[2];
  if (pipe(fds) != 0) return 98;
  fflush(nullptr);
  pid_t pid = fork();
  if (pid == 0) {
    close(fds[0]);
    dup2(fds[1], 1);
    close(fds[1]);
    // testinput2's "tables=3" modifier makes pcre2test read a hardcoded
    // relative filename, "./testbtables" (RunTest handles this the same
    // way, by cp'ing testbtables into its own cwd before running test 2).
    if (chdir(UPSTREAM_TESTDATA_DIR) != 0) _exit(97);
    std::vector<char *> argv = BuildArgv(args);
    int rc = harvest_pcre2test(static_cast<int>(argv.size()), argv.data());
    fflush(nullptr);  // _exit() skips atexit-flushed stdio buffers
    _exit(rc);
  }
  close(fds[1]);
  char buf[65536];
  ssize_t n;
  while ((n = read(fds[0], buf, sizeof(buf))) > 0) {
    out->insert(out->end(), buf, buf + n);
  }
  close(fds[0]);
  int status = 0;
  waitpid(pid, &status, 0);
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  return -WTERMSIG(status);  // negative: killed by a signal
}

struct FileSpec {
  const char *tag;     // memo key, e.g. "test1"
  const char *input;   // testinput file name
  const char *output;  // expected testoutput file name
  bool error_append;   // test 2: append the "-error" stdout run
};

struct RunData {
  int status;                       // exit code, or negative signal number
  std::vector<std::string> blocks;  // segmented actual output
};

std::vector<std::string> SplitBlocks(const std::vector<char> &data);

// Runs `spec`'s pcre2test invocation(s) once per process and memoizes the
// segmented output. Everything sharing this process (the recording test and
// all of its block tests, under the harness's manifest batching) shares one
// run; a fresh process re-records (~30 ms).
const RunData &GetRun(const FileSpec &spec) {
  static auto *memo = new std::map<std::string, RunData>();
  auto it = memo->find(spec.tag);
  if (it != memo->end()) return it->second;

  std::vector<char> raw;
  RunData d;
  d.status = RunPcre2TestToPipe(
      {"-q", "-8", TestDataPath(spec.input), "/dev/stdout"}, &raw);

  if (spec.error_append && d.status == 0) {
    // RunTest appends the output of a standalone "-error" invocation before
    // diffing. Runs in its own child: pcre2test is single-run-per-process.
    std::vector<char> tail;
    int rc2 = RunPcre2TestToPipe(
        {"-q", "-8", "-error", "-80,-62,-2,-1,0,100,101,191,300"}, &tail);
    if (rc2 != 0) d.status = rc2;
    raw.insert(raw.end(), tail.begin(), tail.end());
  }

  // Fault injection for validating the NOT_REACHED semantics (suite
  // self-test only): pretend the child died after 200 output bytes.
  if (getenv("HARVEST_PCRE2_FAULT_TRUNCATE") != nullptr) {
    if (raw.size() > 200) raw.resize(200);
    d.status = -9;
  }

  d.blocks = SplitBlocks(raw);
  return memo->emplace(spec.tag, std::move(d)).first->second;
}

// ---------------------------------------------------------------------------
// Block segmentation: a block is a maximal run of non-blank lines; runs of
// blank lines separate blocks (awk 'BEGIN{RS=""}' paragraph semantics, which
// is how the 1:1 input/output block correspondence was established).

std::vector<std::string> SplitBlocks(const std::vector<char> &data) {
  std::vector<std::string> blocks;
  std::string cur;
  size_t i = 0;
  const size_t n = data.size();
  while (i < n) {
    size_t j = i;
    while (j < n && data[j] != '\n') j++;
    const bool has_nl = j < n;
    std::string line(data.begin() + i, data.begin() + j + (has_nl ? 1 : 0));
    i = j + (has_nl ? 1 : 0);

    if (line == "\n" || line.empty()) {
      if (!cur.empty()) {
        blocks.push_back(cur);
        cur.clear();
      }
    } else {
      cur += line;
    }
  }
  if (!cur.empty()) blocks.push_back(cur);
  return blocks;
}

std::vector<std::string> ExpectedBlocks(const FileSpec &spec) {
  return SplitBlocks(ReadFile(TestDataPath(spec.output)));
}

std::string FirstLine(const std::string &block) {
  const size_t nl = block.find('\n');
  return nl == std::string::npos ? block : block.substr(0, nl);
}

// Counts expected blocks at test-registration time. This drives the Range()
// instantiations below, fixing the reported test set - and therefore the
// harness's denominator - independently of how far a given library under
// test actually gets.
int CountExpectedBlocks(const char *output_name) {
  return static_cast<int>(
      SplitBlocks(ReadFile(TestDataPath(output_name))).size());
}

std::string BlockName(const testing::TestParamInfo<int> &info) {
  char buf[16];
  snprintf(buf, sizeof(buf), "Block%04d", info.param);
  return buf;
}

// ---------------------------------------------------------------------------
// The 14 file specs (RunTest's applicable numbered tests; see
// upstream/README.md for what is excluded and why).

constexpr FileSpec kTest1{"test1", "testinput1", "testoutput1", false};
constexpr FileSpec kTest2{"test2", "testinput2", "testoutput2", true};
constexpr FileSpec kTest4{"test4", "testinput4", "testoutput4", false};
constexpr FileSpec kTest6{"test6", "testinput6", "testoutput6", false};
constexpr FileSpec kTest9{"test9", "testinput9", "testoutput9", false};
constexpr FileSpec kTest15{"test15", "testinput15", "testoutput15", false};
constexpr FileSpec kTest16{"test16", "testinput16", "testoutput16", false};
constexpr FileSpec kTest20{"test20", "testinput20", "testoutput20", false};
constexpr FileSpec kTest21{"test21", "testinput21", "testoutput21", false};
constexpr FileSpec kTest22{"test22", "testinput22", "testoutput22-8", false};
constexpr FileSpec kTest24{"test24", "testinput24", "testoutput24", false};
constexpr FileSpec kTest25{"test25", "testinput25", "testoutput25", false};
constexpr FileSpec kTest26{"test26", "testinput26", "testoutput26", false};
constexpr FileSpec kTest27{"test27", "testinput27", "testoutput27", false};

// Whole-file run: executes (and caches) the pcre2test run, then checks run
// health and output structure. Byte-level content is checked per block.
void CheckFileRun(const FileSpec &spec) {
  const RunData &run = GetRun(spec);
  ASSERT_GE(run.status, 0) << "pcre2test crashed with signal " << -run.status
                           << " on " << spec.input;
  EXPECT_EQ(run.status, 0) << "pcre2test exited nonzero on " << spec.input;

  const std::vector<std::string> expected = ExpectedBlocks(spec);
  ASSERT_FALSE(expected.empty()) << "missing " << spec.output;
  EXPECT_EQ(run.blocks.size(), expected.size())
      << "output block structure differs from " << spec.output;
}

// Per-block comparison against the cached run.
class BlockTestBase : public testing::TestWithParam<int> {
 protected:
  void Check(const FileSpec &spec) {
    const RunData &run = GetRun(spec);
    const std::vector<std::string> expected = ExpectedBlocks(spec);
    const std::vector<std::string> &actual = run.blocks;
    const size_t i = static_cast<size_t>(GetParam());
    ASSERT_LT(i, expected.size()) << "expected-file/block-count skew";

    if (i >= actual.size()) {
      FAIL() << "NOT_REACHED: output ends after " << actual.size()
             << " blocks (run status " << run.status
             << "); expected block starts\n  " << FirstLine(expected[i]);
    }
    if (FirstLine(actual[i]) != FirstLine(expected[i])) {
      FAIL() << "misaligned output at block " << i
             << ": expected block starts\n  " << FirstLine(expected[i])
             << "\nbut actual output has\n  " << FirstLine(actual[i])
             << "\n(an earlier divergence changed the output structure)";
    }
    EXPECT_EQ(actual[i], expected[i]);
  }
};

}  // namespace

// One recording test + one per-block suite per ported file. The macro's
// Range() reads the expected file once at test-registration time.
#define PCRE2_FILE_SUITE(Name, spec)                                        \
  TEST(Pcre2FileRun, Name) { CheckFileRun(spec); }                          \
  class Name : public BlockTestBase {};                                     \
  TEST_P(Name, Matches) { Check(spec); }                                    \
  INSTANTIATE_TEST_SUITE_P(                                                 \
      Blocks, Name, testing::Range(0, CountExpectedBlocks(spec.output)),    \
      BlockName)

// Test 1: Main non-UTF, non-UCP functionality (compatible with Perl >= 5.10).
PCRE2_FILE_SUITE(Test1MainNonUtf, kTest1);

// Test 2: API, errors, internals and non-Perl stuff (incl. the appended
// "-error" run; its output blocks are part of testoutput2).
PCRE2_FILE_SUITE(Test2ApiErrorsInternals, kTest2);

// Test 4: UTF and Unicode property support.
PCRE2_FILE_SUITE(Test4Utf, kTest4);

// Tests 5, 7, and 10 are deliberately not ported: disassembler
// segfault/infinite loop on XCLASS+B patterns in this build; see
// upstream/README.md.

// Test 6: DFA matching, main non-UTF, non-UCP functionality.
PCRE2_FILE_SUITE(Test6DfaMainNonUtf, kTest6);

// Test 9: Specials for the basic 8-bit library.
PCRE2_FILE_SUITE(Test9Specials8Bit, kTest9);

// Test 15: Non-JIT limits and other non-JIT tests.
PCRE2_FILE_SUITE(Test15NonJitLimits, kTest15);

// Test 16: JIT-specific features when JIT is not available.
PCRE2_FILE_SUITE(Test16JitUnavailable, kTest16);

// Test 20: Serialization and code copy tests.
PCRE2_FILE_SUITE(Test20Serialization, kTest20);

// Test 21: \C tests without UTF (supported for DFA matching).
PCRE2_FILE_SUITE(Test21BackslashCNonUtf, kTest21);

// Test 22: \C tests with UTF (output differs per code-unit width, hence the
// "-8"-suffixed expected file).
PCRE2_FILE_SUITE(Test22BackslashCUtf, kTest22);

// Test 24: Non-UTF pattern conversion tests.
PCRE2_FILE_SUITE(Test24PatternConversionNonUtf, kTest24);

// Test 25: UTF pattern conversion tests.
PCRE2_FILE_SUITE(Test25PatternConversionUtf, kTest25);

// Test 26: Unicode property tests (compatible with Perl >= 5.38).
PCRE2_FILE_SUITE(Test26UnicodeProperty, kTest26);

// Test 27: Auto-generated Unicode property tests.
PCRE2_FILE_SUITE(Test27UnicodePropertyAutogenerated, kTest27);
