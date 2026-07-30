// See scenario_record.h for the design rationale.
//
// This file is C++ but exports a plain C interface: the translation units that
// include the upstream test sources must stay C. Every entry point is noexcept
// and swallows exceptions, because unwinding through upstream's C frames would
// be undefined behaviour.
//
// Transport: the recording child streams scenario starts through a pipe and
// the parent keeps the parsed record in process-local memory. There is no
// disk cache — everything that shares a process shares one recording run.
// Under the HARVEST harness, gtest_suite/manifest.json batches a recording
// test and its per-scenario tests into one process invocation; standalone
// standard-gtest usage works unchanged, at the cost of re-recording once per
// separate invocation.
#include "scenario_record.h"

#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

using namespace std::chrono_literals;

// The recording child's deadline is the harness-provided soft timeout for the
// CURRENT invocation: the harness computes it from gtest_suite/manifest.json
// (batch budget), subtracts its own grace, and exports it as
// HARVEST_TEST_SOFT_TIMEOUT_SECS. Reaping the child at this deadline lets the
// parent keep the partial record it has already drained; an external kill of
// the whole process would lose every scenario verdict at once. The manifest
// (via the harness) is the single authority on time; nothing here hardcodes a
// budget. The fallback below only applies when the binary is run by hand
// without the harness.
constexpr std::chrono::milliseconds kStandaloneFallback{60000};

// Write end of the recording pipe; valid only inside the recording child.
int g_pipe_wr = -1;

// Signature of the upstream function being recorded, as handed over from C.
using BodyFn = int (*)();

struct Record {
  std::array<sr_verdict_t, SR_MAX_SCENARIOS> verdicts{};
  int max_observed = -1;
  bool completed = false;
};

// Parsed records by tag, plus the record the sr_* accessors read. Process
// lifetime; gtest runs tests sequentially so no locking is needed.
std::map<std::string, Record>& Memo() {
  static auto* memo = new std::map<std::string, Record>();
  return *memo;
}
const Record* g_current = nullptr;

bool StartsWith(std::string_view s, std::string_view prefix) {
  return s.substr(0, prefix.size()) == prefix;
}

std::optional<int> ParseInt(std::string_view s) {
  int value = 0;
  const auto* const first = s.data();
  const auto result = std::from_chars(first, first + s.size(), value);
  if (result.ec != std::errc{} || result.ptr == first) return std::nullopt;
  return value;
}

// Parses the drained "S <id>" / "DONE <rc>" stream. A scenario start followed
// by another start (or by DONE) passed; the last one started before an abort
// stays FAILED; ids never started stay NOT_REACHED.
Record ParseRecord(const std::string& data) {
  Record r;
  r.verdicts.fill(SR_NOT_REACHED);
  std::istringstream in(data);
  std::string line;
  std::optional<int> prev;
  while (std::getline(in, line)) {
    if (StartsWith(line, "S ")) {
      const auto id = ParseInt(std::string_view{line}.substr(2));
      if (prev) r.verdicts[*prev] = SR_PASSED;
      prev.reset();
      if (id && *id >= 0 && *id < SR_MAX_SCENARIOS) {
        r.verdicts[*id] = SR_FAILED;  // provisional until superseded
        r.max_observed = std::max(r.max_observed, *id);
        prev = id;
      }
    } else if (StartsWith(line, "DONE")) {
      if (prev) r.verdicts[*prev] = SR_PASSED;
      prev.reset();
      r.completed = true;
    }
  }
  return r;
}

std::chrono::milliseconds ChildBudget() {
  if (const char* env = std::getenv("HARVEST_TEST_SOFT_TIMEOUT_SECS")) {
    if (const auto seconds = ParseInt(env))
      return std::max<std::chrono::milliseconds>(std::chrono::seconds{*seconds},
                                                 1000ms);
  }
  return kStandaloneFallback;
}

// Drains the pipe into `out` until EOF, killing the child if it outlives the
// budget. A library under test can hang rather than crash, and the parent
// must still deliver whatever the child streamed so far: after the SIGKILL
// the write end closes and the drain loop finishes at EOF, so nothing already
// received is lost (the pipe-based equivalent of "persist the partial record
// before reaping").
void DrainWithDeadline(pid_t pid, int rfd, std::string* out) {
  const auto limit = ChildBudget();
  const auto deadline = std::chrono::steady_clock::now() + limit;
  bool killed = false;
  char buf[4096];
  for (;;) {
    struct pollfd pfd = {rfd, POLLIN, 0};
    const int rv = poll(&pfd, 1, 50);
    if (rv > 0) {
      const ssize_t n = read(rfd, buf, sizeof(buf));
      if (n > 0) {
        out->append(buf, static_cast<size_t>(n));
        continue;
      }
      break;  // EOF (all write ends closed) or read error
    }
    if (!killed && std::chrono::steady_clock::now() >= deadline) {
      kill(pid, SIGKILL);
      killed = true;  // keep draining: EOF arrives once the child dies
    }
  }
  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
  }
}

// Runs `body` in a forked child that streams scenario starts down the pipe.
// The child leaves via _exit() so it never runs the parent's atexit handlers
// or re-flushes inherited stdio buffers.
bool RecordInChild(BodyFn body, std::string* out) {
  int fds[2];
  if (pipe(fds) != 0) return false;
  const pid_t pid = fork();
  if (pid < 0) {
    close(fds[0]);
    close(fds[1]);
    return false;
  }
  if (pid == 0) {
    close(fds[0]);
    g_pipe_wr = fds[1];
    const int rc = body();
    char line[32];
    const int len = snprintf(line, sizeof(line), "DONE %d\n", rc);
    if (len > 0) {
      ssize_t ignored = write(g_pipe_wr, line, static_cast<size_t>(len));
      (void)ignored;
    }
    _exit(rc == 0 ? 0 : 1);
  }
  close(fds[1]);
  // A crash or hang in the child is expected here: the partial stream stands.
  DrainWithDeadline(pid, fds[0], out);
  close(fds[0]);
  return true;
}

}  // namespace

extern "C" void sr_scenario_start(int id) noexcept {
  if (g_pipe_wr < 0 || id < 0 || id >= SR_MAX_SCENARIOS) return;
  char line[32];
  const int len = snprintf(line, sizeof(line), "S %d\n", id);
  if (len > 0) {
    // Unbuffered write: a crash right after this call must not lose the trail.
    ssize_t ignored = write(g_pipe_wr, line, static_cast<size_t>(len));
    (void)ignored;
  }
}

extern "C" int sr_run_once(const char* tag, int (*body)(void),
                           void* any_symbol) noexcept try {
  (void)any_symbol;  // legacy parameter: keyed a disk cache that no longer exists
  auto& memo = Memo();
  const auto it = memo.find(tag);
  if (it != memo.end()) {
    g_current = &it->second;
    return 0;
  }

  std::string data;
  if (!RecordInChild(body, &data)) return -1;
  const auto inserted = memo.emplace(tag, ParseRecord(data));
  g_current = &inserted.first->second;
  return 0;
} catch (...) {
  return -1;
}

extern "C" sr_verdict_t sr_verdict(int id) noexcept {
  if (g_current == nullptr || id < 0 || id >= SR_MAX_SCENARIOS)
    return SR_NOT_REACHED;
  return g_current->verdicts[static_cast<std::size_t>(id)];
}

extern "C" int sr_max_observed(void) noexcept {
  return g_current ? g_current->max_observed : -1;
}

extern "C" int sr_run_completed(void) noexcept {
  return (g_current != nullptr && g_current->completed) ? 1 : 0;
}
