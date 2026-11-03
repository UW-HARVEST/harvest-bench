// Tests of the toy executable, through harvest_bin.h.
#include <gtest/gtest.h>

#include <dirent.h>
#include <sys/wait.h>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <harvest_bin.h>

using harvest_bin::Request;
using harvest_bin::run;

static std::string letters(size_t n) {
  std::string s(n, '\0');
  for (size_t i = 0; i < n; i++) s[i] = 'a' + (i % 26);
  return s;
}

TEST(ToyBin, UpperFromStdin) {
  Request q;
  q.args = {"upper"};
  q.stdin_data = "hello\n";
  auto r = run(q);
  ASSERT_TRUE(r.exited(0)) << r.describe() << "\n" << r.err;
  EXPECT_EQ(r.out, "HELLO\n");
  EXPECT_EQ(r.err, "");
}

TEST(ToyBin, UsageNamesTheProgram) {
  // argv[0] is the program name (the manifest's program.bin.name), whatever
  // the artifact's file is called. The line is printed so that the runner's
  // regression test can check the name end to end.
  auto r = run(Request{});
  EXPECT_TRUE(r.exited(2)) << r.describe();
  EXPECT_EQ(r.err.rfind("usage: " + std::string(harvest_bin::name()) + " ", 0), 0u) << r.err;
  std::printf("observed usage line: %s", r.err.c_str());
}

TEST(ToyBin, ErrorExitCodeAndStderr) {
  Request q;
  q.args = {"fail", "boom"};
  auto r = run(q);
  EXPECT_TRUE(r.exited(3)) << r.describe();
  EXPECT_EQ(r.err, std::string(harvest_bin::name()) + ": error: boom\n");
  EXPECT_EQ(r.out, "");
}

TEST(ToyBin, WritesFileInItsWorkingDirectory) {
  Request q;
  q.args = {"write", "out.txt", "payload"};
  auto r = run(q);
  ASSERT_TRUE(r.exited(0)) << r.describe() << r.err;
  auto f = harvest_bin::read_output_file(r, "out.txt");
  ASSERT_TRUE(f.has_value());
  EXPECT_EQ(*f, "payload");
}

static std::string real(const std::string& path) {
  char buf[PATH_MAX];
  return realpath(path.c_str(), buf) ? buf : path;
}

TEST(ToyBin, HomeAndWorkingDirectoryAreThePerRunDirectory) {
  // The real user's HOME (and a ~/.sqliterc in it) must not leak in.
  Request q;
  q.args = {"home"};
  auto r = run(q);
  ASSERT_TRUE(r.exited(0)) << r.describe();
  EXPECT_EQ(r.out, "HOME=" + r.cwd + "\nCWD=" + real(r.cwd) + "\n");
}

TEST(ToyBin, LargeStdinAndStdout) {
  Request q;
  q.args = {"cat"};
  q.stdin_data = letters(32u << 20);
  auto r = run(q);
  ASSERT_TRUE(r.exited(0)) << r.describe();
  EXPECT_EQ(r.out.size(), q.stdin_data->size());
  EXPECT_TRUE(r.out == *q.stdin_data);
}

TEST(ToyBin, OutputCapIsReported) {
  Request q;
  q.args = {"gen", "100000"};
  q.max_output = 1000;
  auto r = run(q);
  EXPECT_TRUE(r.output_truncated);
  EXPECT_EQ(r.out.size(), 1000u);
}

static int open_fds() {
  int n = 0;
  if (DIR* d = opendir("/proc/self/fd")) {
    while (readdir(d)) n++;
    closedir(d);
  }
  return n;
}

TEST(ToyBin, ManyShortRunsLeakNothing) {
  run(Request{});  // first use: one-time setup (scratch root) happens here
  const int fds = open_fds();
  for (int i = 0; i < 200; i++) {
    Request q;
    q.args = {"upper"};
    q.stdin_data = "x";
    auto r = run(q);
    ASSERT_TRUE(r.exited(0)) << i << ": " << r.describe();
    ASSERT_EQ(r.out, "X");
  }
  EXPECT_EQ(open_fds(), fds) << "descriptor leak";
  // No child is left, not even an unreaped zombie.
  EXPECT_EQ(waitpid(-1, nullptr, WNOHANG), -1);
  EXPECT_EQ(errno, ECHILD);
}

// Abuse probes: a misbehaving executable must neither hang the run nor leave
// processes behind. Skipped unless TOY_ABUSE is set; HardTimeoutWithChild
// then fails on purpose (timeout). The extra argument TOY_ABUSE carries is a
// marker that the regression test searches for in leftover processes.
static const char* abuse() { return std::getenv("TOY_ABUSE"); }

TEST(ToyBinAbuse, ChildKeepsPipesAfterExit) {
  if (!abuse()) GTEST_SKIP() << "set TOY_ABUSE=<marker>";
  Request q;
  q.args = {"bg", abuse()};
  auto r = run(q);
  EXPECT_TRUE(r.exited(0)) << r.describe();
  EXPECT_EQ(r.out, "started\n");
  EXPECT_LT(r.secs, 5.0);  // the drain grace, not the child's lifetime
}

TEST(ToyBinAbuse, SoftTimeoutWithChild) {
  if (!abuse()) GTEST_SKIP() << "set TOY_ABUSE=<marker>";
  Request q;
  q.args = {"hang", abuse()};
  q.timeout_secs = 2;
  auto r = run(q);
  EXPECT_TRUE(r.timed_out);
  EXPECT_EQ(r.out, "parent waiting\n");
  EXPECT_LT(r.secs, 5.0);
}

TEST(ToyBinAbuse, HardTimeoutWithChild) {
  if (!abuse()) GTEST_SKIP() << "set TOY_ABUSE=<marker>";
  Request q;
  q.args = {"hang", abuse()};
  q.timeout_secs = 1e6;  // only the runner's hard timeout ends this
  run(q);
}
