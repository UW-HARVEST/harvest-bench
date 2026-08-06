//! Harvest Bench runner: builds a test's GoogleTest suite against a shared
//! library under test and executes it according to the suite's invocation
//! manifest.
//!
//! # Suite contract
//! - `gtest_suite/CMakeLists.txt` accepts `-DTEST_LIB_PATH=<abs .so>` (the
//!   library under test) and is otherwise self-contained.
//! - The test executable target is named `harvest_gtest`.
//! - An optional `gtest_suite/manifest.json` declares the process-level
//!   invocation plan (see [`Manifest`]).
//!
//! # Execution model
//! Tests are enumerated with `--gtest_list_tests`, then executed according to
//! the manifest:
//! - `mode: "case"` (and any test not matched by the manifest): one process
//!   per test via `--gtest_filter=<name>`. A crash inside the library under
//!   test takes down only that one test.
//! - `mode: "suite"`: one process for a whole group of tests (aggregate
//!   suites whose sub-cases read a shared in-memory record produced by a fork
//!   inside the suite). Per-case verdicts come from the run's
//!   `--gtest_output=json:` report. If the batch process dies, no report
//!   exists and every planned test in the batch is recorded as failed.
//!
//! The soft deadline of the current invocation is exported to the suite as
//! `HARVEST_TEST_SOFT_TIMEOUT_SECS`; aggregate suites use it to bound their
//! recording children.

use std::fs;
use std::path::{Path, PathBuf};
use std::process::{Command, Output, Stdio};
use std::sync::mpsc;
use std::time::{Duration, Instant};
use wait_timeout::ChildExt;

pub type Error = Box<dyn std::error::Error + Send + Sync>;
pub type Result<T> = std::result::Result<T, Error>;

/// Required name of the suite's test executable target.
pub const GTEST_BINARY_NAME: &str = "harvest_gtest";

/// Name of the invocation-plan manifest inside a suite directory.
pub const MANIFEST_FILE: &str = "manifest.json";

/// Grace the runner waits beyond an invocation's budget before SIGKILLing it.
const KILL_GRACE_SECS: u64 = 5;

/// Environment variable for shared library search paths.
#[cfg(target_os = "macos")]
const LD_LIBRARY_PATH_ENV: &str = "DYLD_LIBRARY_PATH";
#[cfg(not(target_os = "macos"))]
const LD_LIBRARY_PATH_ENV: &str = "LD_LIBRARY_PATH";

// ---------------------------------------------------------------------------
// Manifest

/// How a manifest entry is executed.
#[derive(serde::Deserialize, serde::Serialize, Clone, Copy, PartialEq, Eq, Debug)]
#[serde(rename_all = "lowercase")]
pub enum InvocationMode {
    /// One process per matched test; the budget applies to each test.
    Case,
    /// One process for all matched tests together; the budget applies to the
    /// whole invocation and per-case verdicts come from the JSON report.
    Suite,
}

/// One planned process-level invocation.
#[derive(serde::Deserialize, Clone, Debug)]
pub struct InvocationSpec {
    pub mode: InvocationMode,
    /// gtest filter expression (`:`-separated positive patterns, `*`/`?`
    /// wildcards). Passed verbatim to `--gtest_filter` for `suite` entries.
    pub filter: String,
    /// Measured C-baseline seconds for this invocation. The granted timeout
    /// is `max(budget * default_factor, min_seconds)`.
    pub budget: f64,
    #[serde(default)]
    pub note: Option<String>,
}

/// The invocation-plan manifest shipped with a suite (`manifest.json`).
///
/// Tests are assigned to the first entry whose `filter` matches (manifest
/// order); unmatched tests fall back to one process per test with the
/// caller's fallback timeout.
#[derive(serde::Deserialize, Debug)]
pub struct Manifest {
    #[serde(default = "default_version")]
    pub version: u32,
    #[serde(default = "default_factor")]
    pub default_factor: f64,
    #[serde(default = "default_min_seconds")]
    pub min_seconds: f64,
    #[serde(default)]
    pub invocations: Vec<InvocationSpec>,
}

fn default_version() -> u32 {
    1
}

fn default_factor() -> f64 {
    3.0
}

fn default_min_seconds() -> f64 {
    10.0
}

impl Manifest {
    /// Timeout in seconds granted to an invocation with the given C baseline.
    pub fn timeout_secs(&self, budget: f64) -> u64 {
        (budget * self.default_factor).max(self.min_seconds).ceil() as u64
    }
}

/// Loads `manifest.json` from a suite directory. Returns `Ok(None)` when the
/// file does not exist; a malformed file is an error.
pub fn load_manifest(suite_dir: &Path) -> Result<Option<Manifest>> {
    let path = suite_dir.join(MANIFEST_FILE);
    let raw = match fs::read_to_string(&path) {
        Ok(raw) => raw,
        Err(_) => return Ok(None),
    };
    let manifest: Manifest = serde_json::from_str(&raw)
        .map_err(|e| format!("malformed {}: {}", path.display(), e))?;
    Ok(Some(manifest))
}

// ---------------------------------------------------------------------------
// Filter matching (gtest positive-filter subset: `:`-separated patterns,
// `*` = any run of characters, `?` = any single character)

/// Matches one glob pattern (with `*` and `?`) against a full test name.
pub fn glob_matches(pattern: &str, name: &str) -> bool {
    let p: Vec<char> = pattern.chars().collect();
    let n: Vec<char> = name.chars().collect();
    let (mut pi, mut ni) = (0usize, 0usize);
    let (mut star_pi, mut star_ni) = (usize::MAX, 0usize);
    while ni < n.len() {
        if pi < p.len() && (p[pi] == '?' || p[pi] == n[ni]) {
            pi += 1;
            ni += 1;
        } else if pi < p.len() && p[pi] == '*' {
            star_pi = pi;
            star_ni = ni;
            pi += 1;
        } else if star_pi != usize::MAX {
            pi = star_pi + 1;
            star_ni += 1;
            ni = star_ni;
        } else {
            return false;
        }
    }
    while pi < p.len() && p[pi] == '*' {
        pi += 1;
    }
    pi == p.len()
}

/// Matches a gtest filter expression (positive patterns only) against a name.
pub fn filter_matches(filter: &str, name: &str) -> bool {
    filter
        .split(':')
        .filter(|p| !p.is_empty())
        .any(|p| glob_matches(p, name))
}

// ---------------------------------------------------------------------------
// Execution planning

/// One planned process-level invocation: a `suite` batch with its assigned
/// tests, or a single `case` test.
#[derive(Debug, Clone, serde::Serialize)]
pub struct PlannedInvocation {
    pub mode: InvocationMode,
    pub filter: String,
    pub tests: Vec<String>,
    pub timeout_secs: u64,
    pub note: Option<String>,
}

/// The full process-level plan for one suite run: batches first (manifest
/// order), then per-case invocations (test listing order).
#[derive(Debug, Default, serde::Serialize)]
pub struct ExecutionPlan {
    pub invocations: Vec<PlannedInvocation>,
}

/// Assigns every listed test to the first matching manifest entry (manifest
/// order); unmatched tests run per-case with the fallback timeout.
pub fn build_execution_plan(
    test_names: &[String],
    manifest: Option<&Manifest>,
    fallback_timeout: u64,
) -> ExecutionPlan {
    let case = |name: &String, timeout_secs: u64, note: Option<String>| PlannedInvocation {
        mode: InvocationMode::Case,
        filter: name.clone(),
        tests: vec![name.clone()],
        timeout_secs,
        note,
    };

    let Some(manifest) = manifest else {
        return ExecutionPlan {
            invocations: test_names
                .iter()
                .map(|n| case(n, fallback_timeout, None))
                .collect(),
        };
    };

    let mut batch_slots: Vec<Vec<String>> = vec![Vec::new(); manifest.invocations.len()];
    let mut cases = Vec::new();
    for name in test_names {
        match manifest
            .invocations
            .iter()
            .position(|s| filter_matches(&s.filter, name))
        {
            Some(i) => {
                let spec = &manifest.invocations[i];
                match spec.mode {
                    InvocationMode::Suite => batch_slots[i].push(name.clone()),
                    InvocationMode::Case => {
                        cases.push(case(name, manifest.timeout_secs(spec.budget), spec.note.clone()))
                    }
                }
            }
            None => cases.push(case(name, fallback_timeout, None)),
        }
    }

    let mut invocations = Vec::new();
    for (i, tests) in batch_slots.into_iter().enumerate() {
        if tests.is_empty() {
            continue;
        }
        let spec = &manifest.invocations[i];
        invocations.push(PlannedInvocation {
            mode: InvocationMode::Suite,
            filter: spec.filter.clone(),
            tests,
            timeout_secs: manifest.timeout_secs(spec.budget),
            note: spec.note.clone(),
        });
    }
    invocations.extend(cases);
    ExecutionPlan { invocations }
}

// ---------------------------------------------------------------------------
// Verdicts

/// Why a test failed. Structured so consumers classify without grepping
/// failure prose; the human-readable detail stays in `TestVerdict::failure`.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize)]
#[serde(rename_all = "snake_case")]
pub enum FailureCause {
    /// The gtest JSON report recorded assertion failure(s).
    Assertion,
    /// A per-case process exited nonzero without crashing.
    NonZeroExit { code: Option<i32> },
    /// The process died on a signal (segfault, abort, ...).
    Signal { signal: i32 },
    /// The runner killed the invocation at its time budget.
    TimedOut { budget_secs: u64 },
    /// The batch process died without writing a JSON report; nothing is
    /// known about this test beyond "its invocation did not survive".
    NoReport,
    /// The report exists but this planned test is absent from it (the run
    /// ended early).
    MissingVerdict,
    /// The test binary could not be spawned at all.
    SpawnFailed,
}

impl FailureCause {
    /// Short stable label, for compact displays.
    pub fn label(&self) -> &'static str {
        match self {
            FailureCause::Assertion => "assertion",
            FailureCause::NonZeroExit { .. } => "exit",
            FailureCause::Signal { .. } => "signal",
            FailureCause::TimedOut { .. } => "timeout",
            FailureCause::NoReport => "no-report",
            FailureCause::MissingVerdict => "missing",
            FailureCause::SpawnFailed => "spawn",
        }
    }
}

/// Outcome of one gtest test.
#[derive(Debug, Clone, Copy, PartialEq, serde::Serialize)]
#[serde(rename_all = "snake_case")]
pub enum TestStatus {
    Passed,
    Skipped,
    Failed(FailureCause),
}

/// How an invocation's process ended.
#[non_exhaustive]
#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize)]
#[serde(rename_all = "snake_case")]
pub enum ExitKind {
    Clean { code: Option<i32> },
    Signaled { signal: i32 },
    TimedOut { budget_secs: u64 },
    SpawnFailed,
}

/// Verdict for one gtest test.
#[derive(Debug, Clone, serde::Serialize)]
pub struct TestVerdict {
    pub name: String,
    pub status: TestStatus,
    /// Failure detail (assertion text, crash/timeout reason). Empty on pass.
    #[serde(skip_serializing_if = "String::is_empty")]
    pub failure: String,
    /// Test runtime in seconds, when the gtest JSON report supplied it.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub time_secs: Option<f64>,
    /// Index into `RunReport::invocations` of the process run that produced
    /// this verdict.
    pub invocation: usize,
}

impl TestVerdict {
    pub fn passed(&self) -> bool {
        matches!(self.status, TestStatus::Passed | TestStatus::Skipped)
    }
    pub fn skipped(&self) -> bool {
        matches!(self.status, TestStatus::Skipped)
    }
    pub fn failed(&self) -> bool {
        matches!(self.status, TestStatus::Failed(_))
    }
    pub fn cause(&self) -> Option<FailureCause> {
        match self.status {
            TestStatus::Failed(c) => Some(c),
            _ => None,
        }
    }
}

/// Record of what one process-level invocation DID — only the facts the run
/// produced. What was *planned* (mode, filter, tests, timeout, note) is not
/// repeated here: outcomes are index-aligned with
/// `RunReport::plan.invocations`.
#[derive(Debug, Clone, serde::Serialize)]
pub struct InvocationOutcome {
    pub exit: ExitKind,
    pub duration_secs: f64,
    /// Tail of the process's stderr; kept only when the invocation died
    /// abnormally (crash/timeout/no report).
    #[serde(skip_serializing_if = "String::is_empty")]
    pub stderr_tail: String,
    /// Verdict counts for this invocation (derivable from the verdicts;
    /// carried so event-time displays need not scan).
    pub passed: usize,
    pub skipped: usize,
    pub total: usize,
}

/// Outcome of a whole suite run: the static report. Accumulated from the
/// same events the streaming API emits; `Display` renders the default
/// human-readable summary.
///
/// `plan` is the executed plan itself (including each invocation's full test
/// list); `outcomes[i]` records what running `plan.invocations[i]` did.
#[derive(Debug, Default, serde::Serialize)]
pub struct RunReport {
    pub plan: ExecutionPlan,
    pub outcomes: Vec<InvocationOutcome>,
    pub verdicts: Vec<TestVerdict>,
}

impl RunReport {
    pub fn passed(&self) -> usize {
        self.verdicts
            .iter()
            .filter(|v| matches!(v.status, TestStatus::Passed))
            .count()
    }
    pub fn skipped(&self) -> usize {
        self.verdicts.iter().filter(|v| v.skipped()).count()
    }
    pub fn failed(&self) -> usize {
        self.verdicts.iter().filter(|v| v.failed()).count()
    }
    pub fn all_passed(&self) -> bool {
        self.failed() == 0
    }
}

impl std::fmt::Display for RunReport {
    /// The default static rendering: one summary line, then one line per
    /// failure with its structured cause and the first line of detail.
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(
            f,
            "total: {} tests, {} passed, {} failed, {} skipped",
            self.verdicts.len(),
            self.passed(),
            self.failed(),
            self.skipped()
        )?;
        for v in self.verdicts.iter().filter(|v| v.failed()) {
            let cause = v.cause().map(|c| c.label()).unwrap_or("failed");
            let first = v.failure.lines().next().unwrap_or("");
            writeln!(f, "  FAIL [{}] {} - {}", cause, v.name, first)?;
        }
        Ok(())
    }
}

// ---------------------------------------------------------------------------
// CMake plumbing

fn run_command(mut cmd: Command, what: &str) -> Result<Output> {
    let output = cmd
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .output()
        .map_err(|e| format!("failed to run {}: {}", what, e))?;
    if !output.status.success() {
        return Err(format!(
            "{} failed:\n{}",
            what,
            String::from_utf8_lossy(&output.stderr)
        )
        .into());
    }
    Ok(output)
}

/// Configures and builds a suite against `lib_path`, returning the test
/// binary path.
pub fn build_suite(suite_dir: &Path, lib_path: &Path, build_dir: &Path) -> Result<PathBuf> {
    fs::create_dir_all(build_dir)?;
    let lib_path = lib_path
        .canonicalize()
        .map_err(|e| format!("library not found at {}: {}", lib_path.display(), e))?;

    let mut configure = Command::new("cmake");
    configure
        .arg("-S")
        .arg(suite_dir)
        .arg("-B")
        .arg(build_dir)
        .arg("-DCMAKE_BUILD_TYPE=Release")
        .arg(format!("-DTEST_LIB_PATH={}", lib_path.display()));
    run_command(configure, "cmake configure (suite)")?;

    let mut build = Command::new("cmake");
    build.arg("--build").arg(build_dir).arg("--parallel");
    run_command(build, "cmake build (suite)")?;

    let bin = build_dir.join(GTEST_BINARY_NAME);
    if !bin.exists() {
        return Err(format!(
            "test binary not found at {} (the suite must define an executable target named '{}')",
            bin.display(),
            GTEST_BINARY_NAME
        )
        .into());
    }
    Ok(bin.canonicalize().unwrap_or(bin))
}

/// Builds a test's reference C library (`test_case/CMakeLists.txt`) and
/// returns the path of the produced shared library.
pub fn build_c_reference(test_case_dir: &Path, build_dir: &Path) -> Result<PathBuf> {
    fs::create_dir_all(build_dir)?;

    let mut configure = Command::new("cmake");
    configure
        .arg("-S")
        .arg(test_case_dir)
        .arg("-B")
        .arg(build_dir)
        .arg("-DCMAKE_BUILD_TYPE=Release")
        .arg("-DCMAKE_POLICY_VERSION_MINIMUM=3.10");
    run_command(configure, "cmake configure (test_case)")?;

    let mut build = Command::new("cmake");
    build.arg("--build").arg(build_dir).arg("--parallel");
    run_command(build, "cmake build (test_case)")?;

    // The test_case build convention places the shared library in the build
    // directory root.
    let mut candidates: Vec<PathBuf> = fs::read_dir(build_dir)?
        .filter_map(|e| e.ok())
        .map(|e| e.path())
        .filter(|p| {
            p.extension()
                .map(|ext| ext == "so" || ext == "dylib")
                .unwrap_or(false)
        })
        .collect();
    candidates.sort();
    candidates
        .into_iter()
        .next()
        .ok_or_else(|| {
            format!(
                "no shared library produced in {} by the test_case build",
                build_dir.display()
            )
            .into()
        })
}

// ---------------------------------------------------------------------------
// gtest process plumbing

/// Enumerates test names via `--gtest_list_tests`.
///
/// Listing format: suite lines start at column 0 and end with `.`; test lines
/// are indented. Both may carry trailing `# TypeParam/GetParam` comments.
pub fn list_gtest_tests(gtest_bin: &Path, ld_library_path: &str) -> Result<Vec<String>> {
    let output = Command::new(gtest_bin)
        .arg("--gtest_list_tests")
        .env(LD_LIBRARY_PATH_ENV, ld_library_path)
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .output()
        .map_err(|e| format!("failed to list gtest tests: {}", e))?;
    if !output.status.success() {
        return Err(format!(
            "--gtest_list_tests failed:\n{}",
            String::from_utf8_lossy(&output.stderr)
        )
        .into());
    }

    let stdout = String::from_utf8_lossy(&output.stdout);
    let mut tests = Vec::new();
    let mut suite = String::new();
    for line in stdout.lines() {
        let entry = line.split('#').next().unwrap_or("");
        if entry.trim().is_empty() {
            continue;
        }
        if !entry.starts_with(' ') {
            if entry.trim_end().ends_with('.') {
                suite = entry.trim().to_string();
            }
        } else if !suite.is_empty() {
            tests.push(format!("{}{}", suite, entry.trim()));
        }
    }

    if tests.is_empty() {
        return Err("gtest suite lists no tests".to_string().into());
    }
    Ok(tests)
}

/// A finished gtest process invocation, with a structured exit kind (never a
/// stringly-typed error: a spawn failure or timeout is data, not an `Err`).
struct ProcessRun {
    exit: ExitKind,
    stdout: Vec<u8>,
    stderr: Vec<u8>,
    duration_secs: f64,
}

/// Spawns the gtest binary with the given extra args and a hard deadline.
///
/// The child gets its own process group so the timeout kill reaps its ENTIRE
/// tree: suites fork() helper children (pcre2test runs, scenario recorders),
/// and killing only the direct child leaves a looping grandchild alive —
/// spinning at 100% CPU and holding the inherited output pipes open forever.
///
/// Both output streams are drained on reader threads WHILE the child runs: a
/// large suite prints more than a pipe buffer holds, and a child blocked on a
/// full pipe never exits and would be killed as a spurious timeout. The drain
/// is deadline-bounded — the runner's liveness must never depend on the
/// tested code releasing its descriptors.
fn run_gtest_process(
    gtest_bin: &Path,
    ld_library_path: &str,
    args: &[String],
    timeout: Duration,
) -> ProcessRun {
    let started = Instant::now();
    let mut cmd = Command::new(gtest_bin);
    cmd.args(args)
        .env(LD_LIBRARY_PATH_ENV, ld_library_path)
        .env(
            "HARVEST_TEST_SOFT_TIMEOUT_SECS",
            timeout.as_secs().to_string(),
        )
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    #[cfg(unix)]
    {
        use std::os::unix::process::CommandExt;
        cmd.process_group(0);
    }
    let mut child = match cmd.spawn() {
        Ok(child) => child,
        Err(e) => {
            return ProcessRun {
                exit: ExitKind::SpawnFailed,
                stdout: Vec::new(),
                stderr: format!("failed to spawn gtest binary: {}", e).into_bytes(),
                duration_secs: started.elapsed().as_secs_f64(),
            }
        }
    };
    let pid = child.id();

    let stdout_rx = spawn_pipe_drain(child.stdout.take());
    let stderr_rx = spawn_pipe_drain(child.stderr.take());

    let wait_result = child.wait_timeout(timeout + Duration::from_secs(KILL_GRACE_SECS));
    let timed_out = matches!(wait_result, Ok(None));
    if timed_out {
        kill_process_group(pid);
        let _ = child.kill();
    }
    let status = child.wait();
    // Reap stray descendants unconditionally, even after a clean exit: a
    // fork()ed grandchild that outlived the child would otherwise hold the
    // pipes open. Data already written survives the kill, so nothing drained
    // so far is lost.
    kill_process_group(pid);
    let deadline = Instant::now() + Duration::from_secs(KILL_GRACE_SECS);
    let stdout = collect_drained(stdout_rx, deadline);
    let stderr = collect_drained(stderr_rx, deadline);
    let duration_secs = started.elapsed().as_secs_f64();

    let exit = if timed_out {
        ExitKind::TimedOut {
            budget_secs: timeout.as_secs(),
        }
    } else {
        match status {
            Ok(status) => {
                #[cfg(unix)]
                {
                    use std::os::unix::process::ExitStatusExt;
                    match status.signal() {
                        Some(signal) => ExitKind::Signaled { signal },
                        None => ExitKind::Clean {
                            code: status.code(),
                        },
                    }
                }
                #[cfg(not(unix))]
                ExitKind::Clean {
                    code: status.code(),
                }
            }
            Err(_) => ExitKind::SpawnFailed,
        }
    };
    ProcessRun {
        exit,
        stdout,
        stderr,
        duration_secs,
    }
}

impl ExitKind {
    /// One-line description used in failure prose.
    fn describe(&self) -> String {
        match self {
            ExitKind::Clean { code } => format!("process exited with {:?}", code),
            ExitKind::Signaled { signal } => format!("process died on signal {}", signal),
            ExitKind::TimedOut { budget_secs } => format!(
                "invocation exceeded its {}s budget (killed {}s later)",
                budget_secs, KILL_GRACE_SECS
            ),
            ExitKind::SpawnFailed => "could not spawn the test binary".to_string(),
        }
    }
}

/// SIGKILLs the child's whole process group (the child was spawned as its
/// own group leader, so the group id is its pid). ESRCH — everything already
/// gone — is the common, fine case.
#[cfg(unix)]
fn kill_process_group(pid: u32) {
    unsafe {
        libc::kill(-(pid as libc::pid_t), libc::SIGKILL);
    }
}

#[cfg(not(unix))]
fn kill_process_group(_pid: u32) {}

/// Streams a pipe through a channel in chunks so the collector can both see
/// partial output and stop waiting at a deadline. The thread exits on EOF;
/// if a stray write-end holder somehow survives the process-group kill, the
/// thread leaks rather than wedging the runner.
fn spawn_pipe_drain(
    pipe: Option<impl std::io::Read + Send + 'static>,
) -> mpsc::Receiver<Vec<u8>> {
    let (tx, rx) = mpsc::channel();
    std::thread::spawn(move || {
        let Some(mut pipe) = pipe else { return };
        let mut buf = [0u8; 65536];
        loop {
            match pipe.read(&mut buf) {
                Ok(0) | Err(_) => break,
                Ok(n) => {
                    if tx.send(buf[..n].to_vec()).is_err() {
                        break;
                    }
                }
            }
        }
    });
    rx
}

/// Collects everything a drain thread has streamed, waiting at most until
/// `deadline` for the pipe to reach EOF.
fn collect_drained(rx: mpsc::Receiver<Vec<u8>>, deadline: Instant) -> Vec<u8> {
    let mut out = Vec::new();
    loop {
        match rx.try_recv() {
            Ok(chunk) => {
                out.extend_from_slice(&chunk);
                continue;
            }
            Err(mpsc::TryRecvError::Disconnected) => break,
            Err(mpsc::TryRecvError::Empty) => {}
        }
        let left = deadline.saturating_duration_since(Instant::now());
        if left.is_zero() {
            break;
        }
        match rx.recv_timeout(left) {
            Ok(chunk) => out.extend_from_slice(&chunk),
            Err(_) => break, // EOF (disconnected) or deadline reached
        }
    }
    out
}

// ---------------------------------------------------------------------------
// JSON report ingestion

struct JsonVerdict {
    passed: bool,
    skipped: bool,
    failure_text: String,
    time_secs: Option<f64>,
}

/// Parses a gtest `--gtest_output=json:` report into (full test name, verdict)
/// pairs. Tolerates the three historical spellings of the failure text key
/// (`failure`, `failures`, `message`).
fn parse_gtest_json_report(raw: &str) -> Result<Vec<(String, JsonVerdict)>> {
    let root: serde_json::Value =
        serde_json::from_str(raw).map_err(|e| format!("invalid JSON: {}", e))?;
    let mut out = Vec::new();
    let suites = root
        .get("testsuites")
        .and_then(|v| v.as_array())
        .ok_or("missing testsuites array")?;
    for suite in suites {
        let suite_name = suite.get("name").and_then(|v| v.as_str()).unwrap_or("");
        let Some(tests) = suite.get("testsuite").and_then(|v| v.as_array()) else {
            continue;
        };
        for t in tests {
            let name = t.get("name").and_then(|v| v.as_str()).unwrap_or("");
            let full = format!("{}.{}", suite_name, name);
            let failures = t.get("failures").and_then(|v| v.as_array());
            let failed = failures.map(|a| !a.is_empty()).unwrap_or(false);
            let skipped = t.get("result").and_then(|v| v.as_str()) == Some("SKIPPED")
                || t.get("skipped")
                    .and_then(|v| v.as_array())
                    .map(|a| !a.is_empty())
                    .unwrap_or(false);
            let failure_text = failures
                .map(|a| {
                    a.iter()
                        .filter_map(|f| {
                            f.get("failure")
                                .or_else(|| f.get("message"))
                                .or_else(|| f.get("failures"))
                                .and_then(|v| v.as_str())
                        })
                        .collect::<Vec<_>>()
                        .join("\n")
                })
                .unwrap_or_default();
            let time_secs = t
                .get("time")
                .and_then(|v| v.as_str())
                .and_then(|s| s.trim_end_matches('s').parse::<f64>().ok());
            out.push((
                full,
                JsonVerdict {
                    passed: !failed,
                    skipped,
                    failure_text,
                    time_secs,
                },
            ));
        }
    }
    Ok(out)
}

// ---------------------------------------------------------------------------
// Execution

/// A live event from a suite run. The static `RunReport` is the fold of this
/// stream — every consumer-visible fact is emitted here first, so a
/// streaming consumer can render ANY display without waiting for the end.
///
/// Marked non_exhaustive: renderers must keep a `_ => {}` arm, so new events
/// are not breaking changes.
#[non_exhaustive]
pub enum Event<'a> {
    /// CMake configure+build of the suite is starting.
    SuiteBuildStarted { suite_dir: &'a Path },
    /// The suite binary is built.
    SuiteBuilt { gtest_bin: &'a Path },
    /// `--gtest_list_tests` enumerated the test set.
    TestsDiscovered { count: usize },
    /// The execution plan is final; invocations run next, in order.
    PlanReady {
        plan: &'a ExecutionPlan,
        from_manifest: bool,
    },
    /// A process-level invocation is about to run.
    InvocationStarted {
        index: usize,
        total: usize,
        invocation: &'a PlannedInvocation,
    },
    /// One test's verdict is known. Fires for batched and per-case tests
    /// alike, once their invocation finishes.
    TestFinished { verdict: &'a TestVerdict },
    /// A process-level invocation finished; its counts are final. Carries
    /// both what was planned and what happened (outcomes do not repeat plan
    /// fields).
    InvocationFinished {
        index: usize,
        total: usize,
        invocation: &'a PlannedInvocation,
        outcome: &'a InvocationOutcome,
    },
}

/// The default line-oriented rendering of an event — what the CLI prints:
/// build/plan milestones and per-invocation progress, one line each.
/// Per-batched-test verdicts are elided (they are in the report). Returns
/// None for events the default display keeps silent.
pub fn default_event_line(event: &Event) -> Option<String> {
    match event {
        Event::SuiteBuildStarted { suite_dir } => {
            Some(format!("[build] {}", suite_dir.display()))
        }
        Event::TestsDiscovered { count } => Some(format!("[plan ] {} tests discovered", count)),
        Event::PlanReady {
            plan,
            from_manifest,
        } => Some(format!(
            "[plan ] {} invocation(s){}",
            plan.invocations.len(),
            if *from_manifest {
                ""
            } else {
                " (no manifest; global timeout)"
            }
        )),
        Event::InvocationStarted { invocation, .. } => match invocation.mode {
            InvocationMode::Suite => Some(format!(
                "[batch] {} ({} tests, timeout {}s)",
                invocation.filter,
                invocation.tests.len(),
                invocation.timeout_secs
            )),
            InvocationMode::Case => None,
        },
        Event::InvocationFinished {
            invocation,
            outcome,
            ..
        } => match invocation.mode {
            InvocationMode::Suite => Some(format!(
                "[batch] {} -> {}/{} passed",
                invocation.filter, outcome.passed, outcome.total
            )),
            InvocationMode::Case => {
                let mark = if outcome.skipped > 0 {
                    "SKIP"
                } else if outcome.passed == outcome.total {
                    "ok"
                } else {
                    "FAIL"
                };
                Some(format!("[case ] {} {}", mark, invocation.filter))
            }
        },
        _ => None,
    }
}

/// Bounded, char-boundary-safe tail of a byte stream, for stderr excerpts.
fn tail_of(bytes: &[u8], max: usize) -> String {
    let s = String::from_utf8_lossy(bytes);
    if s.len() <= max {
        return s.into_owned();
    }
    let mut cut = s.len() - max;
    while !s.is_char_boundary(cut) {
        cut += 1;
    }
    format!("...{}", &s[cut..])
}

fn json_to_verdict(name: &str, v: &JsonVerdict, invocation: usize) -> TestVerdict {
    let status = if v.skipped {
        TestStatus::Skipped
    } else if v.passed {
        TestStatus::Passed
    } else {
        TestStatus::Failed(FailureCause::Assertion)
    };
    TestVerdict {
        name: name.to_string(),
        status,
        failure: v.failure_text.clone(),
        time_secs: v.time_secs,
        invocation,
    }
}

/// Folds an invocation's process run and verdicts into its outcome record.
/// `passed` counts strict passes (skips are tallied separately); stderr is
/// kept only when the process ended abnormally.
fn outcome_of(run: &ProcessRun, verdicts: &[TestVerdict]) -> InvocationOutcome {
    let abnormal = !matches!(run.exit, ExitKind::Clean { .. });
    InvocationOutcome {
        exit: run.exit,
        duration_secs: run.duration_secs,
        stderr_tail: if abnormal {
            tail_of(&run.stderr, 2000)
        } else {
            String::new()
        },
        passed: verdicts
            .iter()
            .filter(|v| matches!(v.status, TestStatus::Passed))
            .count(),
        skipped: verdicts.iter().filter(|v| v.skipped()).count(),
        total: verdicts.len(),
    }
}

/// Runs one `suite`-mode invocation and ingests per-case verdicts from its
/// JSON report. If the process dies without writing a report (crash,
/// timeout), every planned test is recorded as failed — never skipped — so a
/// batch death cannot score better than individual failures.
fn run_batch(
    gtest_bin: &Path,
    ld_library_path: &str,
    inv: &PlannedInvocation,
    inv_index: usize,
) -> (InvocationOutcome, Vec<TestVerdict>) {
    let json_path = gtest_bin
        .parent()
        .unwrap_or_else(|| Path::new("."))
        .join("harvest_batch_report.json");
    let _ = fs::remove_file(&json_path);

    let run = run_gtest_process(
        gtest_bin,
        ld_library_path,
        &[
            format!("--gtest_filter={}", inv.filter),
            format!("--gtest_output=json:{}", json_path.display()),
        ],
        Duration::from_secs(inv.timeout_secs),
    );

    let raw = fs::read_to_string(&json_path).ok();
    let mut parse_error = None;
    let parsed = raw.as_ref().and_then(|raw| match parse_gtest_json_report(raw) {
        Ok(v) => Some(v),
        Err(e) => {
            parse_error = Some(e);
            None
        }
    });

    let mut verdicts = Vec::new();
    match parsed {
        Some(parsed) => {
            for name in &inv.tests {
                match parsed.iter().find(|(n, _)| n == name) {
                    Some((_, v)) => verdicts.push(json_to_verdict(name, v, inv_index)),
                    None => verdicts.push(TestVerdict {
                        name: name.clone(),
                        status: TestStatus::Failed(FailureCause::MissingVerdict),
                        failure: format!(
                            "no verdict in the batch report ({})",
                            run.exit.describe()
                        ),
                        time_secs: None,
                        invocation: inv_index,
                    }),
                }
            }
            // Tests the report contains but the planner did not expect:
            // count them too (the binary's own filter matching is
            // authoritative).
            for (name, v) in &parsed {
                if !inv.tests.contains(name) {
                    verdicts.push(json_to_verdict(name, v, inv_index));
                }
            }
        }
        None => {
            let cause = match run.exit {
                ExitKind::TimedOut { budget_secs } => FailureCause::TimedOut { budget_secs },
                ExitKind::SpawnFailed => FailureCause::SpawnFailed,
                _ => FailureCause::NoReport,
            };
            let failure = match parse_error {
                Some(e) => format!("batch report unparseable: {}", e),
                None => format!("batch produced no report: {}", run.exit.describe()),
            };
            for name in &inv.tests {
                verdicts.push(TestVerdict {
                    name: name.clone(),
                    status: TestStatus::Failed(cause),
                    failure: failure.clone(),
                    time_secs: None,
                    invocation: inv_index,
                });
            }
        }
    }

    let outcome = outcome_of(&run, &verdicts);
    (outcome, verdicts)
}

/// Runs a single test in its own process via `--gtest_filter`.
fn run_case(
    gtest_bin: &Path,
    ld_library_path: &str,
    inv: &PlannedInvocation,
    inv_index: usize,
) -> (InvocationOutcome, Vec<TestVerdict>) {
    let name = &inv.filter;
    let run = run_gtest_process(
        gtest_bin,
        ld_library_path,
        &[format!("--gtest_filter={}", name)],
        Duration::from_secs(inv.timeout_secs),
    );
    let stdout = String::from_utf8_lossy(&run.stdout);

    let verdict = match run.exit {
        ExitKind::Clean { code: Some(0) } => TestVerdict {
            name: name.clone(),
            status: if stdout.contains("[  SKIPPED ]") {
                TestStatus::Skipped
            } else {
                TestStatus::Passed
            },
            failure: String::new(),
            time_secs: None,
            invocation: inv_index,
        },
        exit => {
            let cause = match exit {
                ExitKind::Clean { code } => FailureCause::NonZeroExit { code },
                ExitKind::Signaled { signal } => FailureCause::Signal { signal },
                ExitKind::TimedOut { budget_secs } => FailureCause::TimedOut { budget_secs },
                ExitKind::SpawnFailed => FailureCause::SpawnFailed,
            };
            TestVerdict {
                name: name.clone(),
                status: TestStatus::Failed(cause),
                failure: format!(
                    "{}\nstdout:\n{}\nstderr:\n{}",
                    exit.describe(),
                    stdout,
                    String::from_utf8_lossy(&run.stderr)
                ),
                time_secs: None,
                invocation: inv_index,
            }
        }
    };

    let verdicts = vec![verdict];
    let outcome = outcome_of(&run, &verdicts);
    (outcome, verdicts)
}

/// Executes a plan against a built test binary, emitting events as it goes,
/// and returns the folded report. Takes the plan by value: it becomes part
/// of the report (`RunReport::plan`), keeping outcomes free of plan copies.
pub fn execute_plan(
    gtest_bin: &Path,
    ld_library_path: &str,
    plan: ExecutionPlan,
    mut on_event: impl FnMut(&Event),
) -> RunReport {
    let mut outcomes = Vec::new();
    let mut all_verdicts = Vec::new();
    let total = plan.invocations.len();

    for (index, inv) in plan.invocations.iter().enumerate() {
        on_event(&Event::InvocationStarted {
            index,
            total,
            invocation: inv,
        });
        let (outcome, verdicts) = match inv.mode {
            InvocationMode::Suite => run_batch(gtest_bin, ld_library_path, inv, index),
            InvocationMode::Case => run_case(gtest_bin, ld_library_path, inv, index),
        };
        for v in &verdicts {
            on_event(&Event::TestFinished { verdict: v });
        }
        on_event(&Event::InvocationFinished {
            index,
            total,
            invocation: inv,
            outcome: &outcome,
        });
        outcomes.push(outcome);
        all_verdicts.extend(verdicts);
    }

    RunReport {
        plan,
        outcomes,
        verdicts: all_verdicts,
    }
}

// ---------------------------------------------------------------------------
// High-level API

/// Everything needed to run one suite against one library.
pub struct RunConfig {
    /// Directory containing the suite (CMakeLists.txt, manifest.json, ...).
    pub suite_dir: PathBuf,
    /// The shared library under test.
    pub lib_path: PathBuf,
    /// Where to build the suite. A subdirectory of the caller's choosing;
    /// reused across runs for incremental builds.
    pub build_dir: PathBuf,
    /// Timeout in seconds for tests without a manifest entry.
    pub fallback_timeout: u64,
}

/// Builds the suite, plans, and executes — the static entry point. No
/// events, just the final report: render it with `{}` for the default
/// summary, walk `verdicts`/`invocations` for structured detail, or
/// serialize it for machines.
pub fn run_suite(config: &RunConfig) -> Result<RunReport> {
    run_suite_streaming(config, |_| {})
}

/// The streaming entry point: the same pipeline with every step emitted as
/// an [`Event`] while it happens. Pair it with [`default_event_line`] for
/// the stock display, or match the events for a custom one; the returned
/// report is the fold of exactly the events delivered.
pub fn run_suite_streaming(
    config: &RunConfig,
    mut on_event: impl FnMut(&Event),
) -> Result<RunReport> {
    let manifest = load_manifest(&config.suite_dir)?;
    on_event(&Event::SuiteBuildStarted {
        suite_dir: &config.suite_dir,
    });
    let bin = build_suite(&config.suite_dir, &config.lib_path, &config.build_dir)?;
    on_event(&Event::SuiteBuilt { gtest_bin: &bin });
    let ld_library_path = config
        .lib_path
        .canonicalize()
        .unwrap_or_else(|_| config.lib_path.clone())
        .parent()
        .map(|p| p.to_string_lossy().into_owned())
        .unwrap_or_default();

    let tests = list_gtest_tests(&bin, &ld_library_path)?;
    on_event(&Event::TestsDiscovered { count: tests.len() });
    let plan = build_execution_plan(&tests, manifest.as_ref(), config.fallback_timeout);
    on_event(&Event::PlanReady {
        plan: &plan,
        from_manifest: manifest.is_some(),
    });
    Ok(execute_plan(&bin, &ld_library_path, plan, on_event))
}

/// Detects whether a directory is a test directory (a `test_case/` C source
/// tree plus a `gtest_suite/`).
pub fn is_test_dir(path: &Path) -> bool {
    path.join("test_case").is_dir() && path.join("gtest_suite").is_dir()
}

/// Builds the reference C library of a test directory and runs its suite
/// against it. Build trees go under `<test_dir>/build/`.
pub fn verify_test_dir(
    test_dir: &Path,
    fallback_timeout: u64,
    on_event: impl FnMut(&Event),
) -> Result<RunReport> {
    if !is_test_dir(test_dir) {
        return Err(format!(
            "{} is not a test directory (needs test_case/ and gtest_suite/)",
            test_dir.display()
        )
        .into());
    }
    let c_build = test_dir.join("build").join("c");
    let lib = build_c_reference(&test_dir.join("test_case"), &c_build)?;
    let config = RunConfig {
        suite_dir: test_dir.join("gtest_suite"),
        lib_path: lib,
        build_dir: test_dir.join("build").join("gtest"),
        fallback_timeout,
    };
    run_suite_streaming(&config, on_event)
}

// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn glob_basics() {
        assert!(glob_matches("Foo.Bar", "Foo.Bar"));
        assert!(!glob_matches("Foo.Bar", "Foo.Baz"));
        assert!(glob_matches("Foo.*", "Foo.Bar"));
        assert!(glob_matches("*", "anything"));
        assert!(glob_matches("Blocks/*.Matches/*", "Blocks/Test1.Matches/Block0001"));
        assert!(glob_matches("A?C", "ABC"));
        assert!(!glob_matches("A?C", "AC"));
        assert!(glob_matches("*tail", "long tail"));
        assert!(!glob_matches("*tail", "tail wags"));
    }

    #[test]
    fn filter_multi_pattern() {
        assert!(filter_matches("A.*:B.*", "B.test"));
        assert!(!filter_matches("A.*:B.*", "C.test"));
    }

    #[test]
    fn plan_assignment_first_match_wins() {
        let manifest = Manifest {
            version: 1,
            default_factor: 3.0,
            min_seconds: 10.0,
            invocations: vec![
                InvocationSpec {
                    mode: InvocationMode::Suite,
                    filter: "Agg.Rec:Blocks/*".to_string(),
                    budget: 2.0,
                    note: None,
                },
                InvocationSpec {
                    mode: InvocationMode::Case,
                    filter: "Heavy.Big".to_string(),
                    budget: 84.0,
                    note: None,
                },
            ],
        };
        let tests = vec![
            "Agg.Rec".to_string(),
            "Blocks/T.M/Block0000".to_string(),
            "Heavy.Big".to_string(),
            "Plain.Simple".to_string(),
        ];
        let plan = build_execution_plan(&tests, Some(&manifest), 42);
        assert_eq!(plan.invocations.len(), 3);
        // Batches first (manifest order), then cases (listing order).
        let batch = &plan.invocations[0];
        assert_eq!(batch.mode, InvocationMode::Suite);
        assert_eq!(batch.tests.len(), 2);
        assert_eq!(batch.timeout_secs, 10); // floor
        let heavy = &plan.invocations[1];
        assert_eq!(heavy.mode, InvocationMode::Case);
        assert_eq!(heavy.filter, "Heavy.Big");
        assert_eq!(heavy.timeout_secs, 252); // 84*3
        let plain = &plan.invocations[2];
        assert_eq!(plain.mode, InvocationMode::Case);
        assert_eq!(plain.filter, "Plain.Simple");
        assert_eq!(plain.timeout_secs, 42); // fallback
    }

    #[test]
    fn json_report_parsing() {
        let raw = r#"{
          "tests": 3,
          "testsuites": [
            {
              "name": "Blocks/T",
              "testsuite": [
                {"name": "M/Block0000", "status": "RUN", "result": "COMPLETED", "time": "0.007s"},
                {"name": "M/Block0001", "status": "RUN", "result": "COMPLETED",
                 "failures": [{"failure": "boom", "type": ""}]},
                {"name": "M/Block0002", "status": "RUN", "result": "SKIPPED"}
              ]
            }
          ]
        }"#;
        let v = parse_gtest_json_report(raw).unwrap();
        assert_eq!(v.len(), 3);
        assert!(v[0].1.passed && !v[0].1.skipped);
        assert_eq!(v[0].1.time_secs, Some(0.007));
        assert!(!v[1].1.passed);
        assert_eq!(v[1].1.failure_text, "boom");
        assert!(v[2].1.passed && v[2].1.skipped);
    }
}
