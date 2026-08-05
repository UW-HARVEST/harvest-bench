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
#[derive(serde::Deserialize, Clone, Copy, PartialEq, Eq, Debug)]
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

/// One `mode: "suite"` process invocation with its assigned tests.
#[derive(Debug)]
pub struct Batch {
    pub filter: String,
    pub timeout_secs: u64,
    pub note: Option<String>,
    pub tests: Vec<String>,
}

/// The full process-level plan for one suite run.
#[derive(Debug, Default)]
pub struct ExecutionPlan {
    pub batches: Vec<Batch>,
    /// (test name, timeout seconds)
    pub singles: Vec<(String, u64)>,
}

/// Assigns every listed test to the first matching manifest entry (manifest
/// order); unmatched tests run per-case with the fallback timeout.
pub fn build_execution_plan(
    test_names: &[String],
    manifest: Option<&Manifest>,
    fallback_timeout: u64,
) -> ExecutionPlan {
    let mut plan = ExecutionPlan::default();
    let Some(manifest) = manifest else {
        plan.singles = test_names
            .iter()
            .map(|n| (n.clone(), fallback_timeout))
            .collect();
        return plan;
    };

    let mut batch_slots: Vec<Vec<String>> = vec![Vec::new(); manifest.invocations.len()];
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
                    InvocationMode::Case => plan
                        .singles
                        .push((name.clone(), manifest.timeout_secs(spec.budget))),
                }
            }
            None => plan.singles.push((name.clone(), fallback_timeout)),
        }
    }

    for (i, tests) in batch_slots.into_iter().enumerate() {
        if tests.is_empty() {
            continue;
        }
        let spec = &manifest.invocations[i];
        plan.batches.push(Batch {
            filter: spec.filter.clone(),
            timeout_secs: manifest.timeout_secs(spec.budget),
            note: spec.note.clone(),
            tests,
        });
    }
    plan
}

// ---------------------------------------------------------------------------
// Verdicts

/// Outcome of one gtest test.
#[derive(Debug, Clone, serde::Serialize)]
pub struct TestVerdict {
    pub name: String,
    pub passed: bool,
    pub skipped: bool,
    /// Failure detail (assertion text, crash/timeout reason). Empty on pass.
    #[serde(skip_serializing_if = "String::is_empty")]
    pub failure: String,
    /// Test runtime in seconds, when the gtest JSON report supplied it.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub time_secs: Option<f64>,
}

/// Outcome of a whole suite run.
#[derive(Debug, Default, serde::Serialize)]
pub struct RunReport {
    pub verdicts: Vec<TestVerdict>,
}

impl RunReport {
    pub fn passed(&self) -> usize {
        self.verdicts.iter().filter(|v| v.passed && !v.skipped).count()
    }
    pub fn skipped(&self) -> usize {
        self.verdicts.iter().filter(|v| v.skipped).count()
    }
    pub fn failed(&self) -> usize {
        self.verdicts.iter().filter(|v| !v.passed).count()
    }
    pub fn all_passed(&self) -> bool {
        self.failed() == 0
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
) -> Result<Output> {
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
    let mut child = cmd
        .spawn()
        .map_err(|e| format!("failed to spawn gtest binary: {}", e))?;
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

    if timed_out {
        return Err(format!(
            "invocation exceeded its {}s budget (killed {}s later)",
            timeout.as_secs(),
            KILL_GRACE_SECS
        )
        .into());
    }
    match (wait_result, status) {
        (Ok(Some(_)), Ok(status)) | (_, Ok(status)) => Ok(Output {
            status,
            stdout,
            stderr,
        }),
        (_, Err(e)) => Err(format!("error waiting for gtest: {}", e).into()),
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

/// Progress events, for callers that want to display them.
pub enum Progress<'a> {
    BatchStart {
        filter: &'a str,
        tests: usize,
        timeout_secs: u64,
    },
    BatchDone {
        filter: &'a str,
        passed: usize,
        total: usize,
    },
    CaseDone {
        name: &'a str,
        passed: bool,
        skipped: bool,
    },
}

/// Runs one batch invocation and ingests per-case verdicts from its JSON
/// report. If the process dies without writing a report (crash, timeout),
/// every planned test is recorded as failed — never skipped — so a batch
/// death cannot score better than individual failures.
fn run_batch(gtest_bin: &Path, ld_library_path: &str, batch: &Batch) -> Vec<TestVerdict> {
    let json_path = gtest_bin
        .parent()
        .unwrap_or_else(|| Path::new("."))
        .join("harvest_batch_report.json");
    let _ = fs::remove_file(&json_path);

    let run = run_gtest_process(
        gtest_bin,
        ld_library_path,
        &[
            format!("--gtest_filter={}", batch.filter),
            format!("--gtest_output=json:{}", json_path.display()),
        ],
        Duration::from_secs(batch.timeout_secs),
    );

    let fail_all = |reason: &str| -> Vec<TestVerdict> {
        batch
            .tests
            .iter()
            .map(|name| TestVerdict {
                name: name.clone(),
                passed: false,
                skipped: false,
                failure: format!("batch '{}' produced no report: {}", batch.filter, reason),
                time_secs: None,
            })
            .collect()
    };

    let verdicts = match fs::read_to_string(&json_path) {
        Ok(raw) => match parse_gtest_json_report(&raw) {
            Ok(v) => v,
            Err(e) => return fail_all(&format!("unparseable JSON report: {}", e)),
        },
        Err(_) => {
            let reason = match &run {
                Ok(output) => format!(
                    "process exited with {:?} without writing a report (crash mid-run?)",
                    output.status.code()
                ),
                Err(e) => e.to_string(),
            };
            return fail_all(&reason);
        }
    };

    let mut results = Vec::new();
    for name in &batch.tests {
        match verdicts.iter().find(|(n, _)| n == name) {
            Some((_, v)) => results.push(TestVerdict {
                name: name.clone(),
                passed: v.passed,
                skipped: v.skipped,
                failure: v.failure_text.clone(),
                time_secs: v.time_secs,
            }),
            None => results.push(TestVerdict {
                name: name.clone(),
                passed: false,
                skipped: false,
                failure: "no verdict in the batch report (run ended early?)".to_string(),
                time_secs: None,
            }),
        }
    }
    // Tests the report contains but the planner did not expect: count them
    // too (the binary's own filter matching is authoritative).
    for (name, v) in &verdicts {
        if !batch.tests.contains(name) {
            results.push(TestVerdict {
                name: name.clone(),
                passed: v.passed,
                skipped: v.skipped,
                failure: v.failure_text.clone(),
                time_secs: v.time_secs,
            });
        }
    }
    results
}

/// Runs a single test in its own process via `--gtest_filter`.
fn run_case(
    gtest_bin: &Path,
    ld_library_path: &str,
    name: &str,
    timeout_secs: u64,
) -> TestVerdict {
    match run_gtest_process(
        gtest_bin,
        ld_library_path,
        &[format!("--gtest_filter={}", name)],
        Duration::from_secs(timeout_secs),
    ) {
        Ok(output) => {
            let stdout = String::from_utf8_lossy(&output.stdout);
            let skipped = stdout.contains("[  SKIPPED ]");
            if output.status.success() {
                TestVerdict {
                    name: name.to_string(),
                    passed: true,
                    skipped,
                    failure: String::new(),
                    time_secs: None,
                }
            } else {
                TestVerdict {
                    name: name.to_string(),
                    passed: false,
                    skipped: false,
                    failure: format!(
                        "exit status {:?}\nstdout:\n{}\nstderr:\n{}",
                        output.status.code(),
                        stdout,
                        String::from_utf8_lossy(&output.stderr)
                    ),
                    time_secs: None,
                }
            }
        }
        Err(e) => TestVerdict {
            name: name.to_string(),
            passed: false,
            skipped: false,
            failure: e.to_string(),
            time_secs: None,
        },
    }
}

/// Executes a plan against a built test binary.
pub fn execute_plan(
    gtest_bin: &Path,
    ld_library_path: &str,
    plan: &ExecutionPlan,
    mut progress: impl FnMut(Progress),
) -> RunReport {
    let mut report = RunReport::default();

    for batch in &plan.batches {
        progress(Progress::BatchStart {
            filter: &batch.filter,
            tests: batch.tests.len(),
            timeout_secs: batch.timeout_secs,
        });
        let verdicts = run_batch(gtest_bin, ld_library_path, batch);
        let passed = verdicts.iter().filter(|v| v.passed).count();
        progress(Progress::BatchDone {
            filter: &batch.filter,
            passed,
            total: verdicts.len(),
        });
        report.verdicts.extend(verdicts);
    }

    for (name, timeout_secs) in &plan.singles {
        let verdict = run_case(gtest_bin, ld_library_path, name, *timeout_secs);
        progress(Progress::CaseDone {
            name,
            passed: verdict.passed,
            skipped: verdict.skipped,
        });
        report.verdicts.push(verdict);
    }

    report
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

/// Builds the suite, plans, and executes: the whole pipeline.
pub fn run_suite(config: &RunConfig, progress: impl FnMut(Progress)) -> Result<RunReport> {
    let manifest = load_manifest(&config.suite_dir)?;
    let bin = build_suite(&config.suite_dir, &config.lib_path, &config.build_dir)?;
    let ld_library_path = config
        .lib_path
        .canonicalize()
        .unwrap_or_else(|_| config.lib_path.clone())
        .parent()
        .map(|p| p.to_string_lossy().into_owned())
        .unwrap_or_default();

    let tests = list_gtest_tests(&bin, &ld_library_path)?;
    let plan = build_execution_plan(&tests, manifest.as_ref(), config.fallback_timeout);
    Ok(execute_plan(&bin, &ld_library_path, &plan, progress))
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
    progress: impl FnMut(Progress),
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
    run_suite(&config, progress)
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
        assert_eq!(plan.batches.len(), 1);
        assert_eq!(plan.batches[0].tests.len(), 2);
        assert_eq!(plan.batches[0].timeout_secs, 10); // floor
        assert_eq!(plan.singles.len(), 2);
        assert_eq!(plan.singles[0], ("Heavy.Big".to_string(), 252)); // 84*3
        assert_eq!(plan.singles[1], ("Plain.Simple".to_string(), 42)); // fallback
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
