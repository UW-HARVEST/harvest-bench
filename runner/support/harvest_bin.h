// harvest_bin.h - run the executable under test from a GoogleTest case.
//
// Canonical copy: harvest-bench runner/support/harvest_bin.h. A suite that
// tests an executable keeps an unchanged copy, so the test directory stays
// self-contained.
//
// Contract with the suite's CMakeLists.txt and the runner:
//   HARVEST_BIN_PATH   compile definition, from -DTEST_BIN_PATH: absolute path
//                      of the executable under test.
//   HARVEST_BIN_NAME   compile definition, from -DTEST_BIN_NAME, which the
//                      runner sets to the manifest's program.bin.name: the
//                      argv[0] the executable receives, so output that prints
//                      the program name does not depend on the artifact's file
//                      name. Never hard-code it in the suite.
//   HARVEST_TEST_TMPDIR (env, set by the runner per invocation): scratch root;
//                      every run gets a fresh working directory and HOME under
//                      it. The runner removes it afterwards. Without it (plain
//                      gtest runs), runs go under one per-process directory in
//                      TMPDIR that is removed at normal exit.
//   HARVEST_TEST_SOFT_TIMEOUT_SECS (env, set by the runner): a run's default
//                      timeout is this less one second (at least one second),
//                      else 60 seconds, so a hung executable fails as an
//                      assertion with its partial output before the runner's
//                      hard kill.
//
// Behavior:
//   - stdin is the given bytes or /dev/null, never the test's own stdin.
//   - stdin, stdout and stderr are pumped together with poll(), so large
//     input and output cannot deadlock. Captured output is capped per
//     stream; overflow is reported.
//   - The executable stays in the test's process group: the runner's
//     process-group kill (at its hard timeout, and after every invocation)
//     also ends it and its children. A timeout here SIGKILLs the executable
//     and stops reading after a short grace even if a child still holds the
//     pipes.
//   - Minimal environment: PATH, HOME, TMPDIR, LC_ALL=C, LANG=C, TZ=UTC, and
//     LD_LIBRARY_PATH passed through, plus per-request overrides. No core
//     files.
//   - SIGPIPE is ignored in the test process (writing to an executable that
//     closed its stdin returns EPIPE) and restored to default in the child.
//   - If the executable cannot be started, the child prints the reason to
//     stderr and exits with 127.
#ifndef HARVEST_BIN_H
#define HARVEST_BIN_H

#ifndef HARVEST_BIN_PATH
#error "define HARVEST_BIN_PATH (from -DTEST_BIN_PATH) before including harvest_bin.h"
#endif
#ifndef HARVEST_BIN_NAME
#error "define HARVEST_BIN_NAME (from -DTEST_BIN_NAME) before including harvest_bin.h"
#endif

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <fcntl.h>
#include <ftw.h>
#include <poll.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace harvest_bin {

struct Request {
  std::vector<std::string> args;           // argv[1..]
  std::optional<std::string> stdin_data;   // nullopt = /dev/null
  std::string cwd;                         // empty = fresh scratch directory
  std::map<std::string, std::string> env;  // overrides and additions
  double timeout_secs = 0;                 // 0 = the default (see above)
  size_t max_output = 64u << 20;           // per stream
};

struct Result {
  std::string error;       // nonempty: the helper failed; no exit status
  int exit_code = -1;      // valid when it exited
  int term_signal = 0;     // nonzero when a signal ended it (not timeouts)
  bool timed_out = false;  // the timeout killed it
  bool output_truncated = false;
  std::string out, err;    // captured streams
  std::string cwd;         // the working directory it ran in
  double secs = 0;

  bool exited(int code) const {
    return error.empty() && !timed_out && term_signal == 0 && exit_code == code;
  }
  std::string describe() const {
    if (!error.empty()) return "the helper failed: " + error;
    std::string s;
    if (timed_out) s = "the executable timed out";
    else if (term_signal) s = "the executable died on signal " + std::to_string(term_signal);
    else s = "the executable exited with " + std::to_string(exit_code);
    if (output_truncated) s += " (output truncated)";
    return s;
  }
};

inline const char* path() { return HARVEST_BIN_PATH; }
inline const char* name() { return HARVEST_BIN_NAME; }

namespace detail {

inline double now() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

inline void set_nonblock(int fd) { fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK); }

inline std::string& owned_root() {
  static std::string root;
  return root;
}

inline void remove_owned_root() {
  const std::string& root = owned_root();
  if (root.empty()) return;
  nftw(root.c_str(),
       [](const char* p, const struct stat*, int, struct FTW*) { return ::remove(p); }, 16,
       FTW_DEPTH | FTW_PHYS);
}

// HARVEST_TEST_TMPDIR under the runner. Otherwise one per-process directory
// under TMPDIR, removed at normal exit.
inline std::string scratch_root() {
  const char* env = std::getenv("HARVEST_TEST_TMPDIR");
  if (env && *env) return env;
  std::string& root = owned_root();
  if (root.empty()) {
    const char* base = std::getenv("TMPDIR");
    std::string tmpl = std::string(base && *base ? base : "/tmp") + "/harvest-bin-XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    if (!mkdtemp(buf.data())) return "";
    root = buf.data();
    std::atexit(remove_owned_root);
  }
  return root;
}

inline double default_timeout() {
  const char* soft = std::getenv("HARVEST_TEST_SOFT_TIMEOUT_SECS");
  if (!soft) return 60;
  double s = std::atof(soft) - 1;
  return s > 1 ? s : 1;
}

inline pid_t wait_child(pid_t pid, int* status, int flags) {
  pid_t w;
  do {
    w = waitpid(pid, status, flags);
  } while (w < 0 && errno == EINTR);
  return w;
}

}  // namespace detail

// A fresh directory under the scratch root; empty on failure.
inline std::string make_scratch_dir(const char* tag = "run") {
  std::string root = detail::scratch_root();
  if (root.empty()) return "";
  std::string tmpl = root + "/" + tag + "-XXXXXX";
  std::vector<char> buf(tmpl.begin(), tmpl.end());
  buf.push_back('\0');
  if (!mkdtemp(buf.data())) return "";
  return std::string(buf.data());
}

// Runs the executable under test once.
inline Result run(const Request& req) {
  Result r;
  const double started = detail::now();
  const double timeout = req.timeout_secs > 0 ? req.timeout_secs : detail::default_timeout();
  r.cwd = req.cwd.empty() ? make_scratch_dir("bin") : req.cwd;
  if (r.cwd.empty()) {
    r.error = "cannot create a scratch directory";
    return r;
  }
  const std::string tmp = r.cwd + "/.tmp";
  mkdir(tmp.c_str(), 0700);

  std::map<std::string, std::string> env = {
      {"PATH", "/usr/local/bin:/usr/bin:/bin"},
      {"HOME", r.cwd},
      {"TMPDIR", tmp},
      {"LC_ALL", "C"},
      {"LANG", "C"},
      {"TZ", "UTC"},
  };
  if (const char* ld = std::getenv("LD_LIBRARY_PATH")) env["LD_LIBRARY_PATH"] = ld;
  for (const auto& kv : req.env) env[kv.first] = kv.second;

  // Everything the child needs is built before fork: no allocation after.
  std::vector<std::string> argv_s;
  argv_s.push_back(name());
  argv_s.insert(argv_s.end(), req.args.begin(), req.args.end());
  std::vector<char*> argv;
  for (auto& a : argv_s) argv.push_back(const_cast<char*>(a.c_str()));
  argv.push_back(nullptr);
  std::vector<std::string> env_s;
  for (const auto& kv : env) env_s.push_back(kv.first + "=" + kv.second);
  std::vector<char*> envp;
  for (auto& e : env_s) envp.push_back(const_cast<char*>(e.c_str()));
  envp.push_back(nullptr);

  std::signal(SIGPIPE, SIG_IGN);

  int in_p[2], out_p[2], err_p[2];
  if (pipe2(in_p, O_CLOEXEC) || pipe2(out_p, O_CLOEXEC) || pipe2(err_p, O_CLOEXEC)) {
    r.error = std::string("pipe: ") + std::strerror(errno);
    return r;
  }

  pid_t pid = fork();
  if (pid < 0) {
    r.error = std::string("fork: ") + std::strerror(errno);
    for (int fd : {in_p[0], in_p[1], out_p[0], out_p[1], err_p[0], err_p[1]}) close(fd);
    return r;
  }
  if (pid == 0) {
    // Child: async-signal-safe calls only.
    std::signal(SIGPIPE, SIG_DFL);  // an ignored disposition survives exec
    int stdin_fd = req.stdin_data ? in_p[0] : open("/dev/null", O_RDONLY | O_CLOEXEC);
    dup2(stdin_fd, 0);
    dup2(out_p[1], 1);
    dup2(err_p[1], 2);
    struct rlimit no_core = {0, 0};
    setrlimit(RLIMIT_CORE, &no_core);
    const char* step = "chdir ";
    const char* what = r.cwd.c_str();
    if (chdir(r.cwd.c_str()) == 0) {
      execve(path(), argv.data(), envp.data());
      step = "exec ";
      what = path();
    }
    const char* reason = strerror(errno);
    for (const char* part : {step, what, ": ", reason, "\n"}) {
      ssize_t ignored = write(2, part, strlen(part));
      (void)ignored;
    }
    _exit(127);
  }

  close(in_p[0]);
  close(out_p[1]);
  close(err_p[1]);

  int in_fd = in_p[1];
  if (!req.stdin_data || req.stdin_data->empty()) {
    close(in_fd);
    in_fd = -1;
  } else {
    detail::set_nonblock(in_fd);
  }
  int out_fd = out_p[0], err_fd = err_p[0];
  detail::set_nonblock(out_fd);
  detail::set_nonblock(err_fd);

  // After the executable is gone, read what is left for at most this long:
  // a child it left behind may hold the pipes open.
  const double kDrainGrace = 1.0;
  const double deadline = started + timeout;
  double drain_deadline = 0;
  bool reaped = false;
  int status = 0;
  size_t in_off = 0;
  char buf[65536];

  auto gone = [&]() {
    reaped = true;
    drain_deadline = detail::now() + kDrainGrace;
    if (in_fd >= 0) {
      close(in_fd);
      in_fd = -1;
    }
  };
  auto append = [&](std::string& dst, const char* p, size_t n) {
    size_t room = dst.size() < req.max_output ? req.max_output - dst.size() : 0;
    if (n > room) {
      r.output_truncated = true;
      n = room;
    }
    dst.append(p, n);
  };
  // A helper failure kills the executable; the result reports the failure
  // instead of an exit status.
  auto fail = [&](const char* what) {
    r.error = std::string(what) + ": " + std::strerror(errno);
    if (!reaped) {
      kill(pid, SIGKILL);
      detail::wait_child(pid, &status, 0);
      reaped = true;
    }
  };

  for (;;) {
    if (!reaped) {
      pid_t w = detail::wait_child(pid, &status, WNOHANG);
      if (w == pid) {
        gone();
      } else if (w < 0) {
        fail("waitpid");
        break;
      }
    }
    if (!reaped && detail::now() >= deadline) {
      r.timed_out = true;
      kill(pid, SIGKILL);
      detail::wait_child(pid, &status, 0);
      gone();
    }
    if (reaped && ((out_fd < 0 && err_fd < 0) || detail::now() >= drain_deadline)) break;

    struct pollfd fds[3];
    int nfds = 0, i_in = -1, i_out = -1, i_err = -1;
    if (in_fd >= 0) { i_in = nfds; fds[nfds++] = {in_fd, POLLOUT, 0}; }
    if (out_fd >= 0) { i_out = nfds; fds[nfds++] = {out_fd, POLLIN, 0}; }
    if (err_fd >= 0) { i_err = nfds; fds[nfds++] = {err_fd, POLLIN, 0}; }
    // Short slices, so the exit and the deadlines are noticed promptly.
    int rc = poll(fds, nfds, 50);
    if (rc < 0 && errno != EINTR) {
      fail("poll");
      break;
    }
    if (rc <= 0) continue;
    if (i_in >= 0 && fds[i_in].revents) {
      const std::string& data = *req.stdin_data;
      ssize_t n = write(in_fd, data.data() + in_off, data.size() - in_off);
      if (n > 0) in_off += (size_t)n;
      if ((n < 0 && errno != EAGAIN && errno != EINTR) || in_off == data.size()) {
        close(in_fd);
        in_fd = -1;
      }
    }
    for (int k = 0; k < 2; k++) {
      int idx = k == 0 ? i_out : i_err;
      int& fd = k == 0 ? out_fd : err_fd;
      if (idx < 0 || !fds[idx].revents) continue;
      ssize_t n = read(fd, buf, sizeof buf);
      if (n > 0) {
        append(k == 0 ? r.out : r.err, buf, (size_t)n);
      } else if (n == 0 || (errno != EAGAIN && errno != EINTR)) {
        close(fd);
        fd = -1;
      }
    }
  }
  for (int fd : {in_fd, out_fd, err_fd})
    if (fd >= 0) close(fd);

  if (!r.timed_out && r.error.empty()) {
    if (WIFEXITED(status)) r.exit_code = WEXITSTATUS(status);
    if (WIFSIGNALED(status)) r.term_signal = WTERMSIG(status);
  }
  r.secs = detail::now() - started;
  return r;
}

// Reads a file the executable wrote, relative to its working directory.
inline std::optional<std::string> read_output_file(const Result& r, const std::string& rel) {
  const std::string p = r.cwd + "/" + rel;
  int fd = open(p.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) return std::nullopt;
  std::string s;
  char buf[65536];
  ssize_t n;
  while ((n = read(fd, buf, sizeof buf)) > 0) s.append(buf, (size_t)n);
  close(fd);
  return s;
}

}  // namespace harvest_bin

#endif  // HARVEST_BIN_H
