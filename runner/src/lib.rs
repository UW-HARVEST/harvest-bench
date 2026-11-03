//! Harvest Bench runner: builds a test's GoogleTest suite against the
//! artifacts under test (a shared library, an executable, or both) and
//! executes it according to the suite's invocation manifest.
//!
//! # Suite contract
//! - `gtest_suite/CMakeLists.txt` accepts one absolute path per artifact the
//!   program declares: `-DTEST_LIB_PATH=<.so>` (the library under test) and
//!   `-DTEST_BIN_PATH=<executable>` (the executable under test). With the
//!   executable comes `-DTEST_BIN_NAME=<name>`, the manifest's
//!   `program.bin.name`, which the suite passes to it as `argv[0]`. It is
//!   otherwise self-contained.
//! - The test executable target is named `harvest_gtest`.
//! - An optional `gtest_suite/manifest.json` declares the program (see
//!   [`ProgramSpec`]; absent = library only), the type of each test (see
//!   [`TestTypeSpec`]) and the process-level invocation plan (see
//!   [`Manifest`]).
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
//! recording children, and `harvest_bin.h` to bound each run of the
//! executable. Each invocation also gets a new scratch directory, exported as
//! `HARVEST_TEST_TMPDIR` and removed afterwards (see [`Scratch`]).

mod cmake_api;
mod program;

pub use program::{
    resolve_artifacts, Artifacts, BinSpec, Dimension, LibSpec, ProgramSpec, ResolvedArtifacts,
    TestTypeSpec,
};

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

/// Directory, inside a suite's build directory, that holds the scratch
/// directories of its invocations.
const SCRATCH_PARENT_DIR: &str = "harvest_tmp";

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
#[serde(deny_unknown_fields)]
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
///
/// Unknown fields are errors at every level, so a misspelled key cannot
/// silently change what a run tests.
#[derive(serde::Deserialize, Debug)]
#[serde(deny_unknown_fields)]
pub struct Manifest {
    #[serde(default = "default_factor")]
    pub default_factor: f64,
    #[serde(default = "default_min_seconds")]
    pub min_seconds: f64,
    #[serde(default)]
    pub invocations: Vec<InvocationSpec>,
    /// The program under test. Absent: library only.
    #[serde(default)]
    pub program: Option<ProgramSpec>,
    /// What each test exercises. Tests are assigned to the first
    /// matching entry; unmatched tests get [`ProgramSpec::default_test_type`].
    #[serde(default)]
    pub test_types: Vec<TestTypeSpec>,
    /// Free-form remark for suite authors; the runner ignores it.
    #[serde(default)]
    pub note: Option<String>,
}

fn default_factor() -> f64 {
    3.0
}

fn default_min_seconds() -> f64 {
    10.0
}

impl Manifest {
    /// The declared program, or the library-only one.
    pub fn program(&self) -> ProgramSpec {
        self.program.clone().unwrap_or_else(ProgramSpec::library_only)
    }

    /// The type of one test.
    pub fn test_type_of(&self, name: &str) -> Dimension {
        self.type_rules().type_of(name)
    }

    fn type_rules(&self) -> TypeRules {
        TypeRules {
            rules: self.test_types.clone(),
            default: self.program().default_test_type(),
            missing: Vec::new(),
        }
    }

    fn validate(&self) -> Result<()> {
        let program = self.program();
        program.validate()?;
        for t in &self.test_types {
            t.validate()?;
            if !program.has(t.test_type) {
                return Err(format!(
                    "test_types gives '{}' the type {}, which the program does not declare",
                    t.filter, t.test_type
                )
                .into());
            }
        }
        Ok(())
    }

    /// Timeout in seconds granted to an invocation with the given C baseline.
    pub fn timeout_secs(&self, budget: f64) -> u64 {
        (budget * self.default_factor).max(self.min_seconds).ceil() as u64
    }
}

/// What a run knows about test types: the manifest's `test_types` rules
/// (first match wins, else the program's default type) and the dimensions
/// whose artifact the run was not given.
#[derive(Debug, Clone, Default)]
struct TypeRules {
    rules: Vec<TestTypeSpec>,
    default: Dimension,
    missing: Vec<Dimension>,
}

impl TypeRules {
    fn type_of(&self, name: &str) -> Dimension {
        self.rules
            .iter()
            .find(|t| filter_matches(&t.filter, name))
            .map(|t| t.test_type)
            .unwrap_or(self.default)
    }

    /// The type all tests of an invocation share; None for a mixed batch.
    fn uniform_type(&self, inv: &PlannedInvocation) -> Option<Dimension> {
        let first = self.type_of(inv.tests.first()?);
        inv.tests.iter().all(|t| self.type_of(t) == first).then_some(first)
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
    manifest
        .validate()
        .map_err(|e| format!("invalid {}: {}", path.display(), e))?;
    Ok(Some(manifest))
}

/// Loads a suite's manifest and the program it declares (library only when
/// there is no manifest or no `program` block).
pub fn load_program(suite_dir: &Path) -> Result<(Option<Manifest>, ProgramSpec)> {
    let manifest = load_manifest(suite_dir)?;
    let program = manifest
        .as_ref()
        .map(|m| m.program())
        .unwrap_or_else(ProgramSpec::library_only);
    Ok((manifest, program))
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
    /// The run was not given the artifact this test exercises (its
    /// `test_type`): a program with both a library and an executable, run
    /// with only one of them.
    MissingArtifact,
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
            FailureCause::MissingArtifact => "missing-artifact",
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
    /// What the test exercises.
    pub test_type: Dimension,
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
    /// The artifacts the run was given. Absent only in reports folded by
    /// [`execute_plan`] alone.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub program: Option<Artifacts>,
    /// Declared artifacts the run was not given: the tests of these types
    /// are recorded as failed with [`FailureCause::MissingArtifact`].
    #[serde(skip_serializing_if = "Vec::is_empty")]
    pub missing_artifacts: Vec<Dimension>,
    pub plan: ExecutionPlan,
    pub outcomes: Vec<InvocationOutcome>,
    pub verdicts: Vec<TestVerdict>,
}

/// Verdict counts of one test type.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, serde::Serialize)]
pub struct TypeCounts {
    pub total: usize,
    pub passed: usize,
    pub failed: usize,
    pub skipped: usize,
}

impl RunReport {
    /// Verdict counts per test type, `lib` first; types without tests are
    /// left out.
    pub fn by_type(&self) -> Vec<(Dimension, TypeCounts)> {
        [Dimension::Lib, Dimension::Bin]
            .into_iter()
            .filter_map(|t| {
                let mut c = TypeCounts::default();
                for v in self.verdicts.iter().filter(|v| v.test_type == t) {
                    c.total += 1;
                    match v.status {
                        TestStatus::Passed => c.passed += 1,
                        TestStatus::Skipped => c.skipped += 1,
                        TestStatus::Failed(_) => c.failed += 1,
                    }
                }
                (c.total > 0).then_some((t, c))
            })
            .collect()
    }

    /// Whether the summary splits counts and failures by test type: only
    /// when a test is not a library test, so library-only reports render as
    /// they always have.
    fn shows_types(&self) -> bool {
        self.verdicts.iter().any(|v| v.test_type != Dimension::Lib)
    }

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
    /// The default static rendering: one summary line, per-type lines when
    /// the run has executable tests, missing artifacts, then one line per
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
        let shows_types = self.shows_types();
        if shows_types {
            for (t, c) in self.by_type() {
                writeln!(
                    f,
                    "  {}: {} tests, {} passed, {} failed, {} skipped",
                    t, c.total, c.passed, c.failed, c.skipped
                )?;
            }
        }
        for dim in &self.missing_artifacts {
            writeln!(
                f,
                "  MISSING {} artifact: its tests are recorded as failed",
                dim
            )?;
        }
        for v in self.verdicts.iter().filter(|v| v.failed()) {
            let cause = v.cause().map(|c| c.label()).unwrap_or("failed");
            let first = v.failure.lines().next().unwrap_or("");
            if shows_types {
                writeln!(f, "  FAIL [{}] [{}] {} - {}", cause, v.test_type, v.name, first)?;
            } else {
                writeln!(f, "  FAIL [{}] {} - {}", cause, v.name, first)?;
            }
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

/// Configures and builds a suite, returning the test binary path. Each path
/// in `suite_paths` goes to its CMake variable (`TEST_LIB_PATH`,
/// `TEST_BIN_PATH`) as an absolute path (canonical when it exists); use
/// [`resolve_artifacts`] to check them against the program first. `program`
/// supplies `TEST_BIN_NAME`, the executable's program name.
pub fn build_suite(
    suite_dir: &Path,
    suite_paths: &Artifacts,
    program: &ProgramSpec,
    build_dir: &Path,
) -> Result<PathBuf> {
    fs::create_dir_all(build_dir)?;

    let mut configure = Command::new("cmake");
    configure
        .arg("-S")
        .arg(suite_dir)
        .arg("-B")
        .arg(build_dir)
        .arg("-DCMAKE_BUILD_TYPE=Release");
    for dim in [Dimension::Lib, Dimension::Bin] {
        match suite_paths.get(dim) {
            Some(path) => {
                let path = match path.canonicalize() {
                    Ok(p) => p,
                    Err(_) => std::path::absolute(path)?,
                };
                configure.arg(format!("-D{}={}", dim.cmake_var(), path.display()))
            }
            // Clear a value cached by an earlier configure of this build
            // directory: an undeclared artifact must stay unset.
            None => configure.arg(format!("-U{}", dim.cmake_var())),
        };
    }
    // The manifest is the only source of the program name (argv[0]).
    match &program.bin {
        Some(bin) => configure.arg(format!("-DTEST_BIN_NAME={}", bin.name)),
        None => configure.arg("-UTEST_BIN_NAME"),
    };
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

/// Builds a test's reference C program (`test_case/CMakeLists.txt`) with its
/// default configuration and returns every artifact the program declares
/// (located through the CMake file API: the declared `c_target`, or else the
/// build's only target of the needed type). A declared artifact that the
/// build does not produce is an error.
pub fn build_c_reference(
    test_case_dir: &Path,
    build_dir: &Path,
    program: &ProgramSpec,
) -> Result<Artifacts> {
    fs::create_dir_all(build_dir)?;
    cmake_api::write_query(build_dir)?;

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

    cmake_api::locate_artifacts(build_dir, program)
}

// ---------------------------------------------------------------------------
// Scratch directories

/// The scratch directory of one invocation, exported to the suite as
/// `HARVEST_TEST_TMPDIR`. Each invocation gets a new one, so a directory that
/// could not be removed never reaches a later invocation.
pub struct Scratch {
    path: PathBuf,
}

impl Scratch {
    /// Creates a new scratch directory under `<build_dir>/harvest_tmp/`.
    pub fn create(build_dir: &Path) -> Result<Scratch> {
        static NEXT: std::sync::atomic::AtomicU64 = std::sync::atomic::AtomicU64::new(0);
        let n = NEXT.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
        let path = std::path::absolute(build_dir)?
            .join(SCRATCH_PARENT_DIR)
            .join(format!("{}-{}", std::process::id(), n));
        fs::create_dir_all(&path)
            .map_err(|e| format!("cannot create scratch directory {}: {}", path.display(), e))?;
        Ok(Scratch { path })
    }

    pub fn path(&self) -> &Path {
        &self.path
    }

    /// Removes the directory, after giving back write permission to every
    /// directory in it (a tested program may have taken it away).
    pub fn remove(self) -> Result<()> {
        make_dirs_writable(&self.path);
        match fs::remove_dir_all(&self.path) {
            Err(e) if e.kind() != std::io::ErrorKind::NotFound => Err(format!(
                "cannot remove scratch directory {}: {}",
                self.path.display(),
                e
            )
            .into()),
            _ => Ok(()),
        }
    }
}

/// Adds owner rwx to `path` and every directory below it (not following
/// symlinks), so that their entries can be deleted.
fn make_dirs_writable(path: &Path) {
    let Ok(meta) = fs::symlink_metadata(path) else {
        return;
    };
    if !meta.is_dir() {
        return;
    }
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        let mode = meta.permissions().mode();
        if mode & 0o700 != 0o700 {
            let _ = fs::set_permissions(path, fs::Permissions::from_mode(mode | 0o700));
        }
    }
    if let Ok(entries) = fs::read_dir(path) {
        for entry in entries.filter_map(|e| e.ok()) {
            make_dirs_writable(&entry.path());
        }
    }
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
/// tree: suites fork() helper children (pcre2test runs, scenario recorders,
/// the executable under test and its children), and killing only the direct
/// child leaves a looping grandchild alive — spinning at 100% CPU and
/// holding the inherited output pipes open forever. (A descendant that calls
/// setsid escapes; see doc/executable-tests.md.)
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
    let scratch = Scratch::create(gtest_bin.parent().unwrap_or_else(|| Path::new(".")))
        .inspect_err(|e| eprintln!("warning: {}", e))
        .ok();
    let mut cmd = Command::new(gtest_bin);
    cmd.args(args)
        .env(LD_LIBRARY_PATH_ENV, ld_library_path)
        .env(
            "HARVEST_TEST_SOFT_TIMEOUT_SECS",
            timeout.as_secs().to_string(),
        );
    if let Some(scratch) = &scratch {
        cmd.env("HARVEST_TEST_TMPDIR", scratch.path());
    }
    cmd.stdin(Stdio::null())
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
    if let Some(scratch) = scratch {
        if let Err(e) = scratch.remove() {
            eprintln!("warning: {}", e);
        }
    }

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
    /// The run was not given a declared artifact; the suite gets a
    /// placeholder path instead and the artifact's tests count as failed.
    ArtifactMissing { artifact: Dimension },
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
    /// A process-level invocation is about to run. `test_type` is the type
    /// all its tests share (None for a mixed batch).
    InvocationStarted {
        index: usize,
        total: usize,
        invocation: &'a PlannedInvocation,
        test_type: Option<Dimension>,
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
        test_type: Option<Dimension>,
    },
}

/// The `[bin] ` tag of live display lines; empty for library tests and mixed
/// batches.
fn type_tag(test_type: &Option<Dimension>) -> &'static str {
    match test_type {
        Some(Dimension::Bin) => "[bin] ",
        _ => "",
    }
}

/// The default line-oriented rendering of an event — what the CLI prints:
/// build/plan milestones and per-invocation progress, one line each.
/// Per-batched-test verdicts are elided (they are in the report). Returns
/// None for events the default display keeps silent.
pub fn default_event_line(event: &Event) -> Option<String> {
    match event {
        Event::ArtifactMissing { artifact } => Some(format!(
            "[warn ] no {0} artifact given: the {0} tests run against a placeholder and count as failed",
            artifact
        )),
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
        Event::InvocationStarted {
            invocation,
            test_type,
            ..
        } => match invocation.mode {
            InvocationMode::Suite => Some(format!(
                "[batch] {}{} ({} tests, timeout {}s)",
                type_tag(test_type),
                invocation.filter,
                invocation.tests.len(),
                invocation.timeout_secs
            )),
            InvocationMode::Case => None,
        },
        Event::InvocationFinished {
            invocation,
            outcome,
            test_type,
            ..
        } => match invocation.mode {
            InvocationMode::Suite => Some(format!(
                "[batch] {}{} -> {}/{} passed",
                type_tag(test_type),
                invocation.filter,
                outcome.passed,
                outcome.total
            )),
            InvocationMode::Case => {
                let mark = if outcome.skipped > 0 {
                    "SKIP"
                } else if outcome.passed == outcome.total {
                    "ok"
                } else {
                    "FAIL"
                };
                Some(format!("[case ] {} {}{}", mark, type_tag(test_type), invocation.filter))
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

fn json_to_verdict(name: &str, test_type: Dimension, v: &JsonVerdict, invocation: usize) -> TestVerdict {
    let status = if v.skipped {
        TestStatus::Skipped
    } else if v.passed {
        TestStatus::Passed
    } else {
        TestStatus::Failed(FailureCause::Assertion)
    };
    TestVerdict {
        name: name.to_string(),
        test_type,
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
    let mut outcome = InvocationOutcome {
        exit: run.exit,
        duration_secs: run.duration_secs,
        stderr_tail: if abnormal {
            tail_of(&run.stderr, 2000)
        } else {
            String::new()
        },
        passed: 0,
        skipped: 0,
        total: 0,
    };
    outcome.count(verdicts);
    outcome
}

impl InvocationOutcome {
    fn count(&mut self, verdicts: &[TestVerdict]) {
        self.passed = verdicts
            .iter()
            .filter(|v| matches!(v.status, TestStatus::Passed))
            .count();
        self.skipped = verdicts.iter().filter(|v| v.skipped()).count();
        self.total = verdicts.len();
    }
}

/// Records the verdict of a test whose artifact the run was not given as
/// failed, whatever the test did against the placeholder: a missing
/// artifact never scores. The test's own failure text is kept.
fn fail_missing_artifact(v: &mut TestVerdict) {
    let note = format!("no {} artifact was given to this run", v.test_type);
    v.failure = if v.failure.is_empty() {
        note
    } else {
        format!("{}\n{}", note, v.failure)
    };
    v.status = TestStatus::Failed(FailureCause::MissingArtifact);
}

/// Runs one `suite`-mode invocation and ingests per-case verdicts from its
/// JSON report. If the process dies without writing a report (crash,
/// timeout), every planned test is recorded as failed — never skipped — so a
/// batch death cannot score better than individual failures.
fn run_batch(
    gtest_bin: &Path,
    ld_library_path: &str,
    types: &TypeRules,
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
                    Some((_, v)) => verdicts.push(json_to_verdict(name, types.type_of(name), v, inv_index)),
                    None => verdicts.push(TestVerdict {
                        name: name.clone(),
                        test_type: types.type_of(name),
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
                    verdicts.push(json_to_verdict(name, types.type_of(name), v, inv_index));
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
                    test_type: types.type_of(name),
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
    types: &TypeRules,
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
            test_type: types.type_of(name),
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
                test_type: types.type_of(name),
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
    on_event: impl FnMut(&Event),
) -> RunReport {
    execute(gtest_bin, ld_library_path, plan, &TypeRules::default(), on_event)
}

/// [`execute_plan`] with the run's test types: verdicts and events carry
/// each test's type, and verdicts of a missing artifact's type fail.
fn execute(
    gtest_bin: &Path,
    ld_library_path: &str,
    plan: ExecutionPlan,
    types: &TypeRules,
    mut on_event: impl FnMut(&Event),
) -> RunReport {
    let mut outcomes = Vec::new();
    let mut all_verdicts = Vec::new();
    let total = plan.invocations.len();

    for (index, inv) in plan.invocations.iter().enumerate() {
        let test_type = types.uniform_type(inv);
        on_event(&Event::InvocationStarted {
            index,
            total,
            invocation: inv,
            test_type,
        });
        let (mut outcome, mut verdicts) = match inv.mode {
            InvocationMode::Suite => run_batch(gtest_bin, ld_library_path, types, inv, index),
            InvocationMode::Case => run_case(gtest_bin, ld_library_path, types, inv, index),
        };
        if !types.missing.is_empty() {
            for v in verdicts.iter_mut().filter(|v| types.missing.contains(&v.test_type)) {
                fail_missing_artifact(v);
            }
            outcome.count(&verdicts);
        }
        for v in &verdicts {
            on_event(&Event::TestFinished { verdict: v });
        }
        on_event(&Event::InvocationFinished {
            index,
            total,
            invocation: inv,
            outcome: &outcome,
            test_type,
        });
        outcomes.push(outcome);
        all_verdicts.extend(verdicts);
    }

    RunReport {
        program: None,
        missing_artifacts: types.missing.clone(),
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
    /// The artifacts under test, checked against the suite's program with
    /// [`resolve_artifacts`]: a program with both a library and an executable
    /// may be run with only one of them (the other's tests then fail).
    pub artifacts: Artifacts,
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
    on_event: impl FnMut(&Event),
) -> Result<RunReport> {
    let (manifest, program) = load_program(&config.suite_dir)?;
    run_loaded(config, manifest, &program, on_event)
}

/// [`run_suite_streaming`] with the suite's manifest already loaded.
fn run_loaded(
    config: &RunConfig,
    manifest: Option<Manifest>,
    program: &ProgramSpec,
    mut on_event: impl FnMut(&Event),
) -> Result<RunReport> {
    let artifacts = resolve_artifacts(&config.artifacts, program, true)?;
    for &artifact in &artifacts.missing {
        on_event(&Event::ArtifactMissing { artifact });
    }
    on_event(&Event::SuiteBuildStarted {
        suite_dir: &config.suite_dir,
    });
    let suite_paths = artifacts.suite_paths(program, &config.build_dir)?;
    let bin = build_suite(&config.suite_dir, &suite_paths, program, &config.build_dir).map_err(|e| {
        if artifacts.missing.contains(&Dimension::Lib) {
            format!(
                "{}\n(no lib artifact was given: a suite that links the library at build time \
                 cannot build without one)",
                e
            )
            .into()
        } else {
            e
        }
    })?;
    on_event(&Event::SuiteBuilt { gtest_bin: &bin });
    let ld_library_path = artifacts.ld_library_path();

    let tests = list_gtest_tests(&bin, &ld_library_path)?;
    on_event(&Event::TestsDiscovered { count: tests.len() });
    let plan = build_execution_plan(&tests, manifest.as_ref(), config.fallback_timeout);
    on_event(&Event::PlanReady {
        plan: &plan,
        from_manifest: manifest.is_some(),
    });
    let mut types = manifest.map(|m| m.type_rules()).unwrap_or_default();
    types.missing = artifacts.missing;
    let mut report = execute(&bin, &ld_library_path, plan, &types, on_event);
    report.program = Some(artifacts.given);
    Ok(report)
}

/// Detects whether a directory is a test directory (a `test_case/` C source
/// tree plus a `gtest_suite/`).
pub fn is_test_dir(path: &Path) -> bool {
    path.join("test_case").is_dir() && path.join("gtest_suite").is_dir()
}

/// Builds the reference C program of a test directory and runs its suite
/// against it. Build trees go under `<test_dir>/build/`. The reference must
/// provide every artifact the program declares.
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
    let suite_dir = test_dir.join("gtest_suite");
    let (manifest, program) = load_program(&suite_dir)?;
    let artifacts = build_c_reference(&test_dir.join("test_case"), &test_dir.join("build").join("c"), &program)?;
    let config = RunConfig {
        suite_dir,
        artifacts,
        build_dir: test_dir.join("build").join("gtest"),
        fallback_timeout,
    };
    run_loaded(&config, manifest, &program, on_event)
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
            program: None,
            test_types: Vec::new(),
            note: None,
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

    fn manifest(raw: &str) -> Result<Manifest> {
        let m: Manifest = serde_json::from_str(raw)?;
        m.validate()?;
        Ok(m)
    }

    #[test]
    fn manifest_without_program_is_library_only() {
        let m = manifest(r#"{"invocations":[]}"#).unwrap();
        assert_eq!(m.program(), ProgramSpec::library_only());
        assert_eq!(m.test_type_of("Any.Test"), Dimension::Lib);
    }

    #[test]
    fn manifest_test_types() {
        let m = manifest(
            r#"{
                "program":{"lib":{"c_target":"toy"},"bin":{"c_target":"toyexe","name":"toy"}},
                "test_types":[{"type":"bin","filter":"Shell*:Exe.*"}]}"#,
        )
        .unwrap();
        assert_eq!(m.test_type_of("ShellEcho.Basic"), Dimension::Bin);
        assert_eq!(m.test_type_of("Exe.Usage"), Dimension::Bin);
        assert_eq!(m.test_type_of("Api.Open"), Dimension::Lib);
        let bin_only = manifest(r#"{"program":{"bin":{"name":"x"}}}"#).unwrap();
        assert_eq!(bin_only.test_type_of("Anything"), Dimension::Bin);

        // The live [bin] tag needs one type per invocation.
        let plan = build_execution_plan(
            &["Exe.Usage".to_string(), "Api.Open".to_string()],
            Some(&m),
            30,
        );
        let types = m.type_rules();
        assert_eq!(types.uniform_type(&plan.invocations[0]), Some(Dimension::Bin));
        assert_eq!(types.uniform_type(&plan.invocations[1]), Some(Dimension::Lib));
    }

    #[test]
    fn manifest_rejects() {
        assert!(manifest(r#"{"program":{"bin":{"name":"x"}}}"#).is_ok());
        // A program block must declare something; bin needs a name.
        assert!(manifest(r#"{"program":{}}"#).is_err());
        assert!(manifest(r#"{"program":{"bin":{}}}"#).is_err());
        // Old and unknown spellings are errors, not silently ignored.
        assert!(manifest(r#"{"program":{"kind":"lib"}}"#).is_err());
        assert!(manifest(r#"{"program":{"cli":{"name":"x"}}}"#).is_err());
        // A test type must be a declared dimension.
        assert!(manifest(r#"{"test_types":[{"type":"bin","filter":"*"}]}"#).is_err());
        assert!(manifest(
            r#"{"program":{"bin":{"name":"x"}},"test_types":[{"type":"lib","filter":"*"}]}"#
        )
        .is_err());
        assert!(manifest(r#"{"test_types":[{"type":"cli","filter":"*"}]}"#).is_err());
    }

    #[test]
    fn manifest_unknown_fields_are_errors() {
        assert!(manifest(r#"{"note":"kept for authors"}"#).is_ok());
        assert!(manifest(r#"{"programs":{"bin":{"name":"x"}}}"#).is_err());
        assert!(manifest(r#"{"test_type":[{"type":"lib","filter":"*"}]}"#).is_err());
        assert!(manifest(r#"{"invocations":[{"mode":"case","filter":"A.*","bugdet":1.0}]}"#).is_err());
        assert!(manifest(r#"{"invocations":[{"mode":"case","filter":"A.*","budget":1.0,"note":"x"}]}"#).is_ok());
        assert!(manifest(r#"{"test_types":[{"type":"lib","filter":""}]}"#).is_err());
    }

    #[test]
    fn unlisted_and_unmatched_tests_get_rule_or_default_types() {
        let m = manifest(
            r#"{"program":{"lib":{},"bin":{"name":"x"}},
                "test_types":[{"type":"bin","filter":"Shell.*"}]}"#,
        )
        .unwrap();
        let types = m.type_rules();
        assert_eq!(types.type_of("Shell.Late"), Dimension::Bin);
        assert_eq!(types.type_of("Other.X"), Dimension::Lib, "no rule: the default");
        let mixed = PlannedInvocation {
            mode: InvocationMode::Suite,
            filter: "*".to_string(),
            tests: vec!["Shell.A".to_string(), "Api.B".to_string()],
            timeout_secs: 10,
            note: None,
        };
        assert_eq!(types.uniform_type(&mixed), None, "a mixed batch has no tag");
        // Library-only plans serialize as before.
        let json = serde_json::to_value(build_execution_plan(&["Api.Open".to_string()], None, 30)).unwrap();
        assert_eq!(
            json["invocations"][0].as_object().unwrap().keys().collect::<Vec<_>>(),
            vec!["filter", "mode", "note", "tests", "timeout_secs"]
        );
    }

    #[cfg(unix)]
    #[test]
    fn scratch_removal_survives_read_only_directories() {
        use std::os::unix::fs::PermissionsExt;
        let build = std::env::current_exe()
            .unwrap()
            .parent()
            .unwrap()
            .join(format!("scratch-test-{}", std::process::id()));
        let a = Scratch::create(&build).unwrap();
        let b = Scratch::create(&build).unwrap();
        assert_ne!(a.path(), b.path(), "one directory per invocation");
        let locked = a.path().join("locked");
        fs::create_dir(&locked).unwrap();
        fs::write(locked.join("file"), "x").unwrap();
        fs::set_permissions(&locked, fs::Permissions::from_mode(0o500)).unwrap();
        let a_path = a.path().to_path_buf();
        a.remove().unwrap();
        b.remove().unwrap();
        assert!(!a_path.exists());
        fs::remove_dir_all(&build).unwrap();
    }

    #[test]
    fn missing_artifact_fails_the_test_and_shows_in_the_summary() {
        let mut passed = TestVerdict {
            name: "Exe.Usage".to_string(),
            test_type: Dimension::Bin,
            status: TestStatus::Passed,
            failure: String::new(),
            time_secs: None,
            invocation: 0,
        };
        fail_missing_artifact(&mut passed);
        assert_eq!(passed.cause(), Some(FailureCause::MissingArtifact));
        let report = RunReport {
            missing_artifacts: vec![Dimension::Bin],
            verdicts: vec![passed],
            ..RunReport::default()
        };
        let text = report.to_string();
        assert!(text.contains("  bin: 1 tests, 0 passed, 1 failed, 0 skipped\n"), "{text}");
        assert!(text.contains("  MISSING bin artifact"), "{text}");
        assert!(text.contains("  FAIL [missing-artifact] [bin] Exe.Usage - no bin artifact"), "{text}");
    }

    #[test]
    fn library_only_summary_has_no_type_tags() {
        let report = RunReport {
            verdicts: vec![TestVerdict {
                name: "A.B".to_string(),
                test_type: Dimension::Lib,
                status: TestStatus::Failed(FailureCause::Signal { signal: 11 }),
                failure: "process died on signal 11".to_string(),
                time_secs: None,
                invocation: 0,
            }],
            ..RunReport::default()
        };
        assert_eq!(
            report.to_string(),
            "total: 1 tests, 0 passed, 1 failed, 0 skipped\n  FAIL [signal] A.B - process died on signal 11\n"
        );
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
