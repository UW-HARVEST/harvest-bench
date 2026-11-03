//! Command-line interface of the Harvest Bench runner.

use std::path::PathBuf;
use std::process::ExitCode;

use clap::{Parser, Subcommand};
use harvest_bench::{
    build_suite, default_event_line, is_test_dir, list_gtest_tests, load_program,
    resolve_artifacts, run_suite_streaming, verify_test_dir, Artifacts, Event, RunConfig,
    RunReport, Scratch,
};

#[derive(Parser)]
#[command(name = "harvest-bench", version, about = "Runs a Harvest Bench GoogleTest suite against a shared library and/or executable under test")]
struct Cli {
    #[command(subcommand)]
    command: Cmd,
}

#[derive(Subcommand)]
enum Cmd {
    /// Build a suite against the given artifacts and run it.
    ///
    /// Give one artifact per dimension the suite's program declares. For a
    /// program with both, omitting one runs the other's tests and records
    /// the omitted artifact's tests as failed.
    Run {
        /// The suite directory (contains CMakeLists.txt and manifest.json).
        #[arg(long)]
        suite: PathBuf,
        /// The shared library under test.
        #[arg(long)]
        lib: Option<PathBuf>,
        /// The executable under test.
        #[arg(long)]
        bin: Option<PathBuf>,
        /// Build directory for the suite (default: <suite>/../build/gtest).
        #[arg(long)]
        build_dir: Option<PathBuf>,
        /// Timeout in seconds for tests without a manifest entry.
        #[arg(long, default_value = "60")]
        timeout: u64,
        /// Write the full per-test report to this JSON file.
        #[arg(long)]
        json: Option<PathBuf>,
    },
    /// Build the reference C program of one or more test directories and run
    /// each suite against it. Every test must pass.
    Verify {
        /// Test directories, or a directory that contains test directories.
        paths: Vec<PathBuf>,
        /// Timeout in seconds for tests without a manifest entry.
        #[arg(long, default_value = "60")]
        timeout: u64,
        /// Write the full per-test report to this JSON file.
        #[arg(long)]
        json: Option<PathBuf>,
    },
    /// Run a suite once against the reference artifacts and write a starting
    /// manifest.json with measured per-test baselines (case mode). Group
    /// aggregate suites into `suite` entries by hand afterwards. The program
    /// and test_types of an existing manifest are carried over.
    GenManifest {
        /// The suite directory.
        #[arg(long)]
        suite: PathBuf,
        /// The reference library (normally the C build).
        #[arg(long)]
        lib: Option<PathBuf>,
        /// The reference executable (normally the C build).
        #[arg(long)]
        bin: Option<PathBuf>,
        /// Build directory for the suite (default: <suite>/../build/gtest).
        #[arg(long)]
        build_dir: Option<PathBuf>,
        /// Output path (default: <suite>/manifest.json; refuses to overwrite).
        #[arg(long, short)]
        out: Option<PathBuf>,
    },
}

fn default_build_dir(suite: &PathBuf) -> PathBuf {
    suite
        .parent()
        .map(|p| p.join("build").join("gtest"))
        .unwrap_or_else(|| PathBuf::from("build/gtest"))
}

/// The CLI's live display IS the library's default one.
fn print_event(event: &Event) {
    if let Some(line) = default_event_line(event) {
        println!("{}", line);
    }
}

fn write_json(path: &PathBuf, reports: &[(String, &RunReport)]) -> harvest_bench::Result<()> {
    let value: serde_json::Value = reports
        .iter()
        .map(|(name, r)| (name.clone(), serde_json::to_value(r).unwrap()))
        .collect::<serde_json::Map<String, serde_json::Value>>()
        .into();
    std::fs::write(path, serde_json::to_string_pretty(&value)? + "\n")?;
    Ok(())
}

fn cmd_run(
    suite: PathBuf,
    artifacts: Artifacts,
    build_dir: Option<PathBuf>,
    timeout: u64,
    json: Option<PathBuf>,
) -> harvest_bench::Result<bool> {
    let config = RunConfig {
        build_dir: build_dir.unwrap_or_else(|| default_build_dir(&suite)),
        suite_dir: suite,
        artifacts,
        fallback_timeout: timeout,
    };
    let report = run_suite_streaming(&config, print_event)?;
    print!("{}", report);
    if let Some(path) = json {
        write_json(&path, &[("run".to_string(), &report)])?;
    }
    Ok(report.all_passed())
}

fn cmd_verify(
    paths: Vec<PathBuf>,
    timeout: u64,
    json: Option<PathBuf>,
) -> harvest_bench::Result<bool> {
    // Expand roots into test directories.
    let mut test_dirs = Vec::new();
    for path in paths {
        if is_test_dir(&path) {
            test_dirs.push(path);
        } else if path.is_dir() {
            let mut children: Vec<PathBuf> = std::fs::read_dir(&path)?
                .filter_map(|e| e.ok())
                .map(|e| e.path())
                .filter(|p| is_test_dir(p))
                .collect();
            children.sort();
            if children.is_empty() {
                return Err(format!("{} contains no test directories", path.display()).into());
            }
            test_dirs.extend(children);
        } else {
            return Err(format!("{} is not a directory", path.display()).into());
        }
    }

    let mut all_ok = true;
    let mut reports = Vec::new();
    for dir in &test_dirs {
        let name = dir
            .file_name()
            .map(|n| n.to_string_lossy().into_owned())
            .unwrap_or_else(|| dir.display().to_string());
        println!("=== {} ===", name);
        let report = verify_test_dir(dir, timeout, print_event)?;
        print!("{}", report);
        all_ok &= report.all_passed();
        reports.push((name, report));
    }

    if let Some(path) = json {
        let refs: Vec<(String, &RunReport)> =
            reports.iter().map(|(n, r)| (n.clone(), r)).collect();
        write_json(&path, &refs)?;
    }
    Ok(all_ok)
}

fn cmd_gen_manifest(
    suite: PathBuf,
    artifacts: Artifacts,
    build_dir: Option<PathBuf>,
    out: Option<PathBuf>,
) -> harvest_bench::Result<bool> {
    let out = out.unwrap_or_else(|| suite.join("manifest.json"));
    if out.exists() {
        return Err(format!(
            "{} already exists; pass --out to write elsewhere",
            out.display()
        )
        .into());
    }
    let (existing, program) = load_program(&suite)?;
    if existing.is_some() {
        // Reachable only with --out; keep the measurement honest anyway.
        println!("note: the suite already has a manifest; measuring afresh");
    }
    // The program, test types and note are authored, not measured: carry
    // them over. Measuring needs every declared artifact.
    let (declared, test_types, note) = match existing {
        Some(m) => (m.program, m.test_types, m.note),
        None => (None, Vec::new(), None),
    };
    let build_dir = build_dir.unwrap_or_else(|| default_build_dir(&suite));
    let artifacts = resolve_artifacts(&artifacts, &program, false)?;

    let bin = build_suite(&suite, &artifacts.given, &program, &build_dir)?;
    let ld = artifacts.ld_library_path();
    let tests = list_gtest_tests(&bin, &ld)?;

    // One process, whole suite, JSON timings. Tests of the executable put
    // their working directories under the scratch directory.
    let json_path = build_dir.join("gen_manifest_report.json");
    let scratch = Scratch::create(&build_dir)?;
    let _ = std::fs::remove_file(&json_path);
    let status = std::process::Command::new(&bin)
        .arg(format!("--gtest_output=json:{}", json_path.display()))
        .env("HARVEST_TEST_TMPDIR", scratch.path())
        .env(
            if cfg!(target_os = "macos") {
                "DYLD_LIBRARY_PATH"
            } else {
                "LD_LIBRARY_PATH"
            },
            &ld,
        )
        .status()
        .map_err(|e| format!("failed to run suite: {}", e))?;
    if let Err(e) = scratch.remove() {
        println!("warning: {}", e);
    }
    if !status.success() {
        println!(
            "note: the measurement run reported failures (status {:?}); baselines may be incomplete",
            status.code()
        );
    }
    let raw = std::fs::read_to_string(&json_path)
        .map_err(|e| format!("no JSON report produced: {}", e))?;
    let root: serde_json::Value = serde_json::from_str(&raw)?;

    let mut entries = Vec::new();
    if let Some(suites) = root.get("testsuites").and_then(|v| v.as_array()) {
        for s in suites {
            let sname = s.get("name").and_then(|v| v.as_str()).unwrap_or("");
            if let Some(cases) = s.get("testsuite").and_then(|v| v.as_array()) {
                for t in cases {
                    let tname = t.get("name").and_then(|v| v.as_str()).unwrap_or("");
                    let time = t
                        .get("time")
                        .and_then(|v| v.as_str())
                        .and_then(|x| x.trim_end_matches('s').parse::<f64>().ok())
                        .unwrap_or(0.0);
                    // Round up to a readable value with a small floor.
                    let budget = ((time * 10.0).ceil() / 10.0).max(0.1);
                    entries.push(serde_json::json!({
                        "mode": "case",
                        "filter": format!("{}.{}", sname, tname),
                        "budget": budget,
                    }));
                }
            }
        }
    }
    println!(
        "measured {} of {} listed tests",
        entries.len(),
        tests.len()
    );

    let mut manifest = serde_json::json!({
        "default_factor": 3.0,
        "min_seconds": 10,
        "invocations": entries,
    });
    if let Some(program) = declared {
        manifest["program"] = serde_json::to_value(program)?;
    }
    if !test_types.is_empty() {
        manifest["test_types"] = serde_json::to_value(test_types)?;
    }
    if let Some(note) = note {
        manifest["note"] = note.into();
    }
    std::fs::write(&out, serde_json::to_string_pretty(&manifest)? + "\n")?;
    println!("wrote {}", out.display());
    println!("hint: merge related entries into wildcard or suite-mode groups by hand");
    Ok(true)
}

fn main() -> ExitCode {
    let cli = Cli::parse();
    let result = match cli.command {
        Cmd::Run {
            suite,
            lib,
            bin,
            build_dir,
            timeout,
            json,
        } => cmd_run(suite, Artifacts { lib, bin }, build_dir, timeout, json),
        Cmd::Verify {
            paths,
            timeout,
            json,
        } => cmd_verify(paths, timeout, json),
        Cmd::GenManifest {
            suite,
            lib,
            bin,
            build_dir,
            out,
        } => cmd_gen_manifest(suite, Artifacts { lib, bin }, build_dir, out),
    };
    match result {
        Ok(true) => ExitCode::SUCCESS,
        Ok(false) => ExitCode::from(1),
        Err(e) => {
            eprintln!("error: {}", e);
            ExitCode::from(2)
        }
    }
}
