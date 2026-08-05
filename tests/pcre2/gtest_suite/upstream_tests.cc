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
//     is anchored on the echoed first line.
//
// Containment: a library under test can crash, abort (PR_ABEND) — or hang,
// looping forever without producing output. Every pcre2test child therefore
// runs under a deadline (the harness's HARVEST_TEST_SOFT_TIMEOUT_SECS; 60s
// standalone fallback): the parent drains the pipe with poll() and SIGKILLs
// the child at the deadline, keeping everything already streamed (the
// scenario_record.cc pattern from the lz4/zstd suites). The child also arms
// PR_SET_PDEATHSIG and alarm() so it can never outlive its parent or its
// deadline as an orphan.
//
// Resume: when a run DIES mid-file (crash/abort/deadline), the record does
// not stop there. The runner re-runs pcre2test on a synthesized input that
// replays the earlier #-directive blocks (settings state flows through the
// file) and continues from the first block with no output; a block that
// kills the run even when retried first is marked CRASHED and skipped. So
// one bad block costs exactly its own verdict (plus budget), not every
// verdict after it — while a block past the point where the budget or the
// resume cap ran out still fails as NOT_REACHED, a failure, not a skip: an
// early crash can never score better than a late one, and every block that
// scores PASS was genuinely executed and compared.
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
// cascade into later ones (resume replays directive blocks but not pattern
// blocks, so #save/#load chains keep this caveat). Same accepted caveat as
// the zstd scenario split.

#include <gtest/gtest.h>

#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

#include <algorithm>
#include <cerrno>
#include <chrono>
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
// Time budget. The harness exports the invocation's budget as
// HARVEST_TEST_SOFT_TIMEOUT_SECS (see gtest_suite/manifest.json); it covers
// the primary run plus every resume segment for the file spec being
// recorded. The fallback only applies when the binary runs by hand.

using Clock = std::chrono::steady_clock;

std::chrono::milliseconds OverallBudget() {
  if (const char *env = std::getenv("HARVEST_TEST_SOFT_TIMEOUT_SECS")) {
    char *end = nullptr;
    const long v = std::strtol(env, &end, 10);
    if (end != env && v > 0) return std::chrono::seconds(v);
  }
  return std::chrono::milliseconds(60000);
}

// ---------------------------------------------------------------------------
// Running pcre2test in a forked child (it is single-run-per-process; see
// upstream/README.md). The child's stdout is a pipe; the output file
// argument is /dev/stdout, so both the normal outfile mode and the -error
// mode (bare printf) stream back the same way. The parent must drain the
// pipe WHILE the child runs - the output exceeds the pipe buffer.
//
// The library under test can hang (loop forever without writing), so the
// drain is deadline-bounded: poll, read, SIGKILL the child at the deadline,
// then keep draining — EOF arrives once the child dies and nothing already
// received is lost (the scenario_record.cc pattern from lz4/zstd). The
// child additionally arms PR_SET_PDEATHSIG and alarm() so that even if this
// parent is killed first (harness batch timeout), no orphan survives to
// spin at 100% CPU holding inherited descriptors open.

int RunPcre2TestToPipe(const std::vector<std::string> &args,
                       Clock::time_point deadline, std::vector<char> *out) {
  int fds[2];
  if (pipe(fds) != 0) return 98;
  fflush(nullptr);
  pid_t pid = fork();
  if (pid == 0) {
    close(fds[0]);
    dup2(fds[1], 1);
    close(fds[1]);
#ifdef __linux__
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    if (getppid() == 1) _exit(96);  // parent already gone; die now
#endif
    const long secs = std::chrono::duration_cast<std::chrono::seconds>(
                          deadline - Clock::now())
                          .count();
    alarm(static_cast<unsigned>(std::max(1L, secs + 2)));
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
  bool killed = false;
  char buf[65536];
  for (;;) {
    struct pollfd pfd = {fds[0], POLLIN, 0};
    const int rv = poll(&pfd, 1, 50);
    if (rv > 0) {
      const ssize_t n = read(fds[0], buf, sizeof(buf));
      if (n > 0) {
        out->insert(out->end(), buf, buf + n);
        continue;
      }
      break;  // EOF (all write ends closed) or read error
    }
    if (!killed && Clock::now() >= deadline) {
      kill(pid, SIGKILL);
      killed = true;  // keep draining: EOF arrives once the child dies
    }
  }
  close(fds[0]);
  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
  }
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

// A dead block's stand-in in the record: pcre2test died on this block before
// echoing anything, even when a resume retried it first. Check() reports it
// as CRASHED; it can never equal an expected block.
const char kCrashMarker[] =
    "** HARVEST: pcre2test died processing this block; no output captured\n";

// At most this many pcre2test runs per file spec, so a library that dies on
// every other block cannot burn unbounded time. Each hang also costs its
// segment's slice of the budget, which caps things well before this.
constexpr int kMaxResumeSegments = 16;

// Builds a temporary resume input starting at block `next`: every earlier
// #-directive block is replayed first (pcre2test settings state — #newline,
// #forbid_utf, #pattern … — flows through the file), then the not-yet-covered
// blocks verbatim. The replayed directives' echoes are dropped from the
// resumed output via *skip_out. Pattern blocks are NOT replayed, so test
// 20's #save/#load chains keep their accepted cross-block caveat.
std::string WriteResumeInput(const std::vector<std::string> &in_blocks,
                             size_t next, size_t *skip_out) {
  std::string path = TestDataPath("harvest_resume_XXXXXX");
  std::vector<char> tmpl(path.begin(), path.end());
  tmpl.push_back('\0');
  const int fd = mkstemp(tmpl.data());
  if (fd < 0) return "";
  path.assign(tmpl.data());

  std::string content;
  size_t skip = 0;
  for (size_t i = 0; i < next && i < in_blocks.size(); i++) {
    if (!in_blocks[i].empty() && in_blocks[i][0] == '#') {
      content += in_blocks[i];
      content += '\n';  // blank-line block separator
      skip++;
    }
  }
  for (size_t i = next; i < in_blocks.size(); i++) {
    content += in_blocks[i];
    content += '\n';
  }

  size_t off = 0;
  while (off < content.size()) {
    const ssize_t n = write(fd, content.data() + off, content.size() - off);
    if (n <= 0) {
      close(fd);
      unlink(path.c_str());
      return "";
    }
    off += static_cast<size_t>(n);
  }
  close(fd);
  *skip_out = skip;
  return path;
}

// Runs `spec`'s pcre2test invocation(s) once per process and memoizes the
// segmented output. Everything sharing this process (the recording test and
// all of its block tests, under the harness's manifest batching) shares one
// run; a fresh process re-records (~30 ms).
//
// If a run dies (crash, PR_ABEND, deadline kill), the loop resumes from the
// first input block that produced no output; a block that kills the run even
// when tried first in a resume is recorded as kCrashMarker and skipped. See
// the file comment ("Resume") for the scoring semantics.
const RunData &GetRun(const FileSpec &spec) {
  static auto *memo = new std::map<std::string, RunData>();
  auto it = memo->find(spec.tag);
  if (it != memo->end()) return it->second;

  const Clock::time_point overall_deadline = Clock::now() + OverallBudget();
  const std::vector<std::string> in_blocks =
      SplitBlocks(ReadFile(TestDataPath(spec.input)));

  RunData d;
  d.status = 0;
  size_t next = 0;  // first input block not yet covered by an output block
  // Whether the last committed output ended WITHOUT a terminating blank
  // line. RunTest concatenates the main and "-error" outputs before diffing,
  // so an unterminated final main block and the first -error block form ONE
  // block of the expected file; the append below replicates that merge.
  bool open_tail = false;

  for (int segment = 0; segment < kMaxResumeSegments; segment++) {
    if (next >= in_blocks.size()) break;
    const Clock::time_point now = Clock::now();
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        overall_deadline - now);
    if (remaining < std::chrono::milliseconds(500)) break;

    // The primary run gets the budget minus a reserve for resumes; each
    // resume gets a bounded slice so one more hang cannot eat the rest.
    Clock::time_point deadline;
    if (segment == 0) {
      const auto reserve =
          std::min(std::chrono::milliseconds(3000), remaining / 3);
      deadline = now + (remaining - reserve);
    } else {
      deadline = now + std::min(std::chrono::milliseconds(2000), remaining);
    }

    size_t skip = 0;
    std::string tmp;  // temp resume input; empty for the primary run
    std::string input_path;
    if (segment == 0) {
      input_path = TestDataPath(spec.input);
    } else {
      tmp = WriteResumeInput(in_blocks, next, &skip);
      if (tmp.empty()) break;
      input_path = tmp;
    }

    std::vector<char> raw;
    int rc = RunPcre2TestToPipe({"-q", "-8", input_path, "/dev/stdout"},
                                deadline, &raw);
    if (!tmp.empty()) unlink(tmp.c_str());

    // Fault injection for validating the resume/NOT_REACHED semantics (suite
    // self-test only): pretend the primary run died after 200 output bytes.
    if (segment == 0 && getenv("HARVEST_PCRE2_FAULT_TRUNCATE") != nullptr) {
      if (raw.size() > 200) raw.resize(200);
      rc = -9;
    }

    std::vector<std::string> got = SplitBlocks(raw);
    if (skip > 0) {
      got.erase(got.begin(),
                got.begin() + static_cast<long>(std::min(skip, got.size())));
    }
    if (!got.empty()) {
      open_tail = raw.size() < 2 || raw[raw.size() - 1] != '\n' ||
                  raw[raw.size() - 2] != '\n';
    }
    for (auto &b : got) d.blocks.push_back(std::move(b));
    next += got.size();

    if (rc == 0) {
      // Clean completion covers everything it was given. Any block-count
      // divergence left is a content bug, not a truncation — stop here and
      // let the per-block anchors report it.
      break;
    }
    if (d.status == 0) d.status = rc;  // first failure wins

    if (getenv("HARVEST_PCRE2_NO_RESUME") != nullptr) break;

    if (got.empty() && next < in_blocks.size()) {
      // Died without even echoing its first pending block: that block is the
      // killer (e.g. an infinite loop before any output). Mark it, skip it.
      d.blocks.push_back(kCrashMarker);
      open_tail = false;
      next++;
    }
    // Otherwise the last captured block is partial/suspect and simply fails
    // its comparison; the resume retries from the first silent block.
  }

  if (spec.error_append &&
      Clock::now() + std::chrono::milliseconds(500) < overall_deadline) {
    // RunTest appends the output of a standalone "-error" invocation before
    // diffing. Runs in its own child: pcre2test is single-run-per-process.
    std::vector<char> tail;
    int rc2 =
        RunPcre2TestToPipe({"-q", "-8", "-error", "-80,-62,-2,-1,0,100,101,191,300"},
                           overall_deadline, &tail);
    if (rc2 != 0 && d.status == 0) d.status = rc2;
    std::vector<std::string> tail_blocks = SplitBlocks(tail);
    size_t first = 0;
    if (open_tail && !d.blocks.empty() && !tail_blocks.empty()) {
      d.blocks.back() += tail_blocks[0];  // the RunTest concatenation merge
      first = 1;
    }
    for (size_t i = first; i < tail_blocks.size(); i++) {
      d.blocks.push_back(std::move(tail_blocks[i]));
    }
  }

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
    if (actual[i] == kCrashMarker) {
      FAIL() << "CRASHED: pcre2test died processing this block before"
             << " echoing any output (run status " << run.status
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
