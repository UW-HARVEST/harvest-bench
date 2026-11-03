#pragma once
// Run environment of the CLI scenarios. Upstream shell tests start the
// executable with Tcl `exec`/`open |...` through `test_cli_invocation`, which
// looks for the program in $::cmdlinearg(TESTFIXTURE_HOME). This object makes
// a private directory with an executable link named HARVEST_BIN_NAME (the
// manifest's program.bin.name) to HARVEST_BIN_PATH, so the program sees its
// declared name in argv[0] whatever the artifact file is called. It also
// fixes the environment that the CLI processes inherit, and restores the
// test process afterwards.
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>
extern char **environ;

// Scratch root of an invocation: the runner's HARVEST_TEST_TMPDIR, else TMPDIR.
inline std::string harvest_scratch_root() {
  for (const char *name : {"HARVEST_TEST_TMPDIR", "TMPDIR"}) {
    const char *v = std::getenv(name);
    if (v && *v) return v;
  }
  return "/tmp";
}

struct CliEnvironment {
  std::filesystem::path root, home, tmp, bin;
  std::vector<std::string> saved_env;
  int saved_stdin = -1;
  struct rlimit saved_core {};
  bool core_saved = false;

  CliEnvironment(const char *artifact, const char *name) {
    if (access(artifact, X_OK) != 0)
      throw std::runtime_error(std::string("executable under test is not available: ") + artifact + ": " + std::strerror(errno));
    std::string pattern = harvest_scratch_root() + "/harvest-sqlite-cli-XXXXXX";
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    if (!mkdtemp(buffer.data())) throw std::runtime_error("mkdtemp failed for the CLI environment");
    root = buffer.data();
    home = root / "home"; tmp = root / "tmp"; bin = root / "bin";
    try {
      for (const auto &d : {home, tmp, bin}) std::filesystem::create_directory(d);
      std::filesystem::create_symlink(std::filesystem::absolute(artifact), bin / name);
    } catch (...) {
      std::error_code ignored;
      std::filesystem::remove_all(root, ignored);
      throw;
    }
    // Fixed environment for every CLI process: no ~/.sqliterc, history file
    // or SQLITE_* variable of the caller leaks in.
    for (char **e = environ; *e; ++e) saved_env.emplace_back(*e);
    const char *ld = std::getenv("LD_LIBRARY_PATH");
    std::string ld_value = ld ? ld : "";
    clearenv();
    setenv("PATH", "/usr/local/bin:/usr/bin:/bin", 1);  // as runner/support/harvest_bin.h
    if (ld) setenv("LD_LIBRARY_PATH", ld_value.c_str(), 1);
    setenv("HOME", home.c_str(), 1);
    setenv("TMPDIR", tmp.c_str(), 1);
    setenv("LC_ALL", "C", 1);
    setenv("LANG", "C", 1);
    setenv("TZ", "UTC", 1);
    // A CLI started without redirection must not read the caller's stdin.
    saved_stdin = dup(0);
    int null = open("/dev/null", O_RDONLY);
    if (null >= 0) { dup2(null, 0); close(null); }
    // A crashing CLI must not write core files into the scenario directory.
    if (getrlimit(RLIMIT_CORE, &saved_core) == 0) {
      core_saved = true;
      struct rlimit none {0, saved_core.rlim_max};
      setrlimit(RLIMIT_CORE, &none);
    }
  }
  ~CliEnvironment() {
    if (core_saved) setrlimit(RLIMIT_CORE, &saved_core);
    if (saved_stdin >= 0) { dup2(saved_stdin, 0); close(saved_stdin); }
    clearenv();
    for (auto &e : saved_env) {
      auto eq = e.find('=');
      if (eq != std::string::npos) setenv(e.substr(0, eq).c_str(), e.substr(eq + 1).c_str(), 1);
    }
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
  }
  CliEnvironment(const CliEnvironment &) = delete;
  CliEnvironment &operator=(const CliEnvironment &) = delete;
};
