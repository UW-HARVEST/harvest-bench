// See pv_record.h for the design rationale.
//
// This file is C++ but exports a plain C interface: the translation unit that
// includes pngvalid.c must stay C. Every entry point is noexcept and swallows
// exceptions, because unwinding through upstream's C frames would be
// undefined behavior.
//
// Transport: a forked child streams one line per scenario through a pipe and
// the parent keeps the parsed record in process-local memory. There is no
// disk cache — everything that shares a process shares one recording run.

#include "pv_record.h"

#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

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
constexpr std::chrono::milliseconds kStandaloneFallback{1800000};

// Longest failure message kept per scenario.
constexpr std::size_t kMaxMessage = 300;

// Write end of the reporting pipe; valid only inside a forked child.
int g_pipe_wr = -1;

struct GroupRecord {
    std::vector<pv_verdict_t> verdicts;
    std::vector<std::string> failures;
    // Scenarios the walk reported visiting, or -1 if it never got that far.
    int visited = -1;
    bool recorded = false;
    bool completed = false;
};

GroupRecord& Group(int group) {
    static GroupRecord groups[PV_NGROUPS];
    return groups[group];
}

bool ValidGroup(int group) { return group >= 0 && group < PV_NGROUPS; }

bool ParseInt(std::string_view s, int* out) {
    const auto* const first = s.data();
    const auto result = std::from_chars(first, first + s.size(), *out);
    return result.ec == std::errc{} && result.ptr != first;
}

std::chrono::milliseconds RecordBudget() {
    if (const char* env = std::getenv("HARVEST_TEST_SOFT_TIMEOUT_SECS")) {
        int seconds = 0;
        if (ParseInt(env, &seconds))
            return std::max<std::chrono::milliseconds>(
                std::chrono::seconds{seconds}, 1000ms);
    }
    return kStandaloneFallback;
}

// Drains the pipe into `out` until EOF, killing the child if it outlives the
// budget. A library under test can hang rather than crash, and the parent must
// still deliver whatever the child streamed so far: after the SIGKILL the write
// end closes and the drain loop finishes at EOF, so nothing already received is
// lost.
void DrainWithDeadline(pid_t pid, int rfd, std::chrono::milliseconds limit,
                       std::string* out) {
    const auto deadline = std::chrono::steady_clock::now() + limit;
    bool killed = false;
    char buf[8192];
    for (;;) {
        struct pollfd pfd = {rfd, POLLIN, 0};
        const int rv = poll(&pfd, 1, 50);
        if (rv > 0) {
            const ssize_t n = read(rfd, buf, sizeof(buf));
            if (n > 0) {
                out->append(buf, static_cast<std::size_t>(n));
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

// Runs the walk in a forked child that streams its report down the pipe. The
// child leaves via _exit() so it never runs the parent's atexit handlers or
// re-flushes inherited stdio buffers.
bool RunInChild(int group, int execute, std::chrono::milliseconds limit,
                std::string* out) {
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
        const int rc = harvest_pngvalid_walk(group, execute);
        char line[32];
        const int len = snprintf(line, sizeof(line), "DONE %d\n", rc);
        if (len > 0) {
            ssize_t ignored = write(g_pipe_wr, line, static_cast<std::size_t>(len));
            (void)ignored;
        }
        _exit(rc == 0 ? 0 : 1);
    }
    close(fds[1]);
    // A crash or hang in the child is expected here: the partial stream stands.
    DrainWithDeadline(pid, fds[0], limit, out);
    close(fds[0]);
    return true;
}

void EnsureSize(GroupRecord* g, std::size_t n) {
    if (g->verdicts.size() < n) {
        g->verdicts.resize(n, PV_NOT_REACHED);
        g->failures.resize(n);
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Child side: reporting.

extern "C" void pv_emit_name(int index, const char* name) noexcept {
    if (g_pipe_wr < 0 || index < 0) return;
    std::string line = "N ";
    line += std::to_string(index);
    line += ' ';
    line += (name != nullptr ? name : "unnamed");
    line += '\n';
    ssize_t ignored = write(g_pipe_wr, line.data(), line.size());
    (void)ignored;
}

extern "C" void pv_emit_result(int index, int failed,
                               const char* message) noexcept {
    if (g_pipe_wr < 0 || index < 0) return;
    std::string text = (message != nullptr) ? message : "";
    // One line per scenario: strip the separators out of the payload.
    for (char& c : text) {
        if (c == '\n' || c == '\r') c = ' ';
    }
    if (text.size() > kMaxMessage) text.resize(kMaxMessage);
    std::string line = "R ";
    line += std::to_string(index);
    line += (failed != 0) ? " 1 " : " 0 ";
    line += text;
    line += '\n';
    // Unbuffered write: a crash right after this call must not lose the trail.
    ssize_t ignored = write(g_pipe_wr, line.data(), line.size());
    (void)ignored;
}

extern "C" void pv_emit_count(int count) noexcept {
    if (g_pipe_wr < 0) return;
    const std::string line = "C " + std::to_string(count) + "\n";
    ssize_t ignored = write(g_pipe_wr, line.data(), line.size());
    (void)ignored;
}

// ---------------------------------------------------------------------------
// Parent side: recording.

extern "C" int pv_ensure_recorded(int group, int expected) noexcept try {
    if (!ValidGroup(group)) return -1;
    GroupRecord& g = Group(group);
    if (g.recorded) return 0;

    // Size the table from the checked-in list, so scenarios the run never
    // reaches are still addressable (and reported as NOT_REACHED).
    if (expected > 0) EnsureSize(&g, static_cast<std::size_t>(expected));
    std::string data;
    if (!RunInChild(group, /*execute=*/1, RecordBudget(), &data)) return -1;

    std::istringstream in(data);
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("DONE", 0) == 0) {
            g.completed = true;
            continue;
        }
        if (line.rfind("C ", 0) == 0) {
            int visited = 0;
            if (ParseInt(std::string_view{line}.substr(2), &visited))
                g.visited = visited;
            continue;
        }
        if (line.rfind("R ", 0) != 0) continue;
        const std::size_t sep = line.find(' ', 2);
        if (sep == std::string::npos) continue;
        int index = 0;
        if (!ParseInt(std::string_view{line}.substr(2, sep - 2), &index)) continue;
        if (index < 0) continue;
        EnsureSize(&g, static_cast<std::size_t>(index) + 1);
        const bool failed = line.size() > sep + 1 && line[sep + 1] == '1';
        g.verdicts[static_cast<std::size_t>(index)] = failed ? PV_FAILED : PV_PASSED;
        if (failed && line.size() > sep + 3)
            g.failures[static_cast<std::size_t>(index)] = line.substr(sep + 3);
    }
    g.recorded = true;
    return 0;
} catch (...) {
    return -1;
}

extern "C" int pv_recorded_count(int group) noexcept {
    if (!ValidGroup(group)) return -1;
    return Group(group).visited;
}

extern "C" pv_verdict_t pv_verdict(int group, int index) noexcept {
    if (!ValidGroup(group)) return PV_NOT_REACHED;
    const GroupRecord& g = Group(group);
    if (index < 0 || static_cast<std::size_t>(index) >= g.verdicts.size())
        return PV_NOT_REACHED;
    return g.verdicts[static_cast<std::size_t>(index)];
}

extern "C" const char* pv_failure(int group, int index) noexcept {
    if (!ValidGroup(group)) return "";
    const GroupRecord& g = Group(group);
    if (index < 0 || static_cast<std::size_t>(index) >= g.failures.size()) return "";
    return g.failures[static_cast<std::size_t>(index)].c_str();
}

extern "C" int pv_run_completed(int group) noexcept {
    if (!ValidGroup(group)) return 0;
    return Group(group).completed ? 1 : 0;
}
