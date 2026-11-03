//! End-to-end regression test of the runner on the toy fixture in
//! `runner/fixtures/toy/`: a program with a library and an executable, run
//! against its C reference, with misbehaving executables, without its
//! executable, and through `gen-manifest`.
//!
//! It builds C and C++ code and fetches GoogleTest, so it is ignored by
//! default. Run it with:
//!
//! ```text
//! cargo test --release --test fixtures -- --ignored --nocapture
//! ```

use std::path::{Path, PathBuf};
use std::process::Command;

use harvest_bench::{
    run_suite, verify_test_dir, Artifacts, Dimension, FailureCause, RunConfig, RunReport,
    TestStatus,
};

fn toy() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("fixtures/toy")
}

fn status_of<'a>(report: &'a RunReport, name: &str) -> &'a TestStatus {
    &report
        .verdicts
        .iter()
        .find(|v| v.name == name)
        .unwrap_or_else(|| panic!("no verdict for {name}"))
        .status
}

/// Processes whose command line contains `marker`.
fn processes_with(marker: &str) -> Vec<String> {
    let mut found = Vec::new();
    for entry in std::fs::read_dir("/proc").unwrap().filter_map(|e| e.ok()) {
        if let Ok(raw) = std::fs::read(entry.path().join("cmdline")) {
            let cmdline = String::from_utf8_lossy(&raw).replace('\0', " ");
            if cmdline.contains(marker) {
                found.push(format!("{}: {}", entry.file_name().to_string_lossy(), cmdline));
            }
        }
    }
    found
}

#[test]
#[ignore = "builds the fixture and fetches GoogleTest; run with --ignored"]
fn toy_fixture() {
    let work = Path::new(env!("CARGO_TARGET_TMPDIR")).join("toy_fixture");
    let suite = toy().join("gtest_suite");
    let run = |artifacts: Artifacts, build: &str| {
        run_suite(&RunConfig {
            suite_dir: suite.clone(),
            artifacts,
            build_dir: work.join(build),
            fallback_timeout: 60,
        })
    };

    // The C reference passes; abuse probes are skipped.
    let reference = verify_test_dir(&toy(), 60, |_| {}).unwrap();
    print!("toy (C reference):\n{reference}");
    assert!(reference.all_passed());
    let program = reference.program.clone().unwrap();
    let c_lib = program.lib.unwrap();
    let c_bin = program.bin.unwrap();
    assert!(c_lib.ends_with("libtoy.so"), "c_target picks toy, not aplugin");
    assert!(c_bin.ends_with("bin/driver"), "the file API finds a renamed executable");
    let by_type = reference.by_type();
    assert_eq!((by_type[0].0, by_type[0].1.passed), (Dimension::Lib, 1));
    assert_eq!(by_type[1].0, Dimension::Bin);
    assert_eq!((by_type[1].1.passed, by_type[1].1.skipped), (8, 3));

    // The JSON report carries the new fields, and only those.
    let json = serde_json::to_value(&reference).unwrap();
    assert!(json["program"]["lib"].as_str().unwrap().ends_with("libtoy.so"));
    let usage = json["verdicts"]
        .as_array()
        .unwrap()
        .iter()
        .find(|v| v["name"] == "ToyBin.UsageNamesTheProgram")
        .unwrap();
    assert_eq!(usage["test_type"], "bin");
    assert!(json["plan"]["invocations"][0].get("test_types").is_none());

    // argv[0] is the manifest's bin.name ("toy"), not the file name
    // ("driver"); the suite's CMakeLists has no name of its own.
    let scratch = work.join("argv0_scratch");
    std::fs::create_dir_all(&scratch).unwrap();
    let out = Command::new(toy().join("build/gtest/harvest_gtest"))
        .arg("--gtest_filter=ToyBin.UsageNamesTheProgram")
        .env("HARVEST_TEST_TMPDIR", &scratch)
        .output()
        .unwrap();
    let stdout = String::from_utf8_lossy(&out.stdout);
    assert!(stdout.contains("observed usage line: usage: toy "), "{stdout}");
    std::fs::remove_dir_all(&scratch).unwrap();

    // Misbehaving executables: no hang, nothing left behind. The marker
    // reaches the probes' command lines through TOY_ABUSE.
    let marker = format!("harvest-fixture-probe-{}", std::process::id());
    std::env::set_var("TOY_ABUSE", &marker);
    let abuse = run(
        Artifacts {
            lib: Some(c_lib.clone()),
            bin: Some(c_bin.clone()),
        },
        "abuse",
    );
    std::env::remove_var("TOY_ABUSE");
    let abuse = abuse.unwrap();
    print!("toy abuse probes:\n{abuse}");
    assert_eq!(*status_of(&abuse, "ToyBinAbuse.ChildKeepsPipesAfterExit"), TestStatus::Passed);
    assert_eq!(*status_of(&abuse, "ToyBinAbuse.SoftTimeoutWithChild"), TestStatus::Passed);
    assert!(matches!(
        status_of(&abuse, "ToyBinAbuse.HardTimeoutWithChild"),
        TestStatus::Failed(FailureCause::TimedOut { .. })
    ));
    assert_eq!(abuse.failed(), 1);
    std::thread::sleep(std::time::Duration::from_millis(500));
    assert_eq!(processes_with(&marker), Vec::<String>::new(), "leftover processes");

    // No executable given: library tests still count, executable tests fail
    // as missing (the child reports the placeholder's exec error).
    let no_bin = run(Artifacts::lib(&c_lib), "no_bin").unwrap();
    print!("toy without bin:\n{no_bin}");
    assert!(no_bin.program.as_ref().unwrap().bin.is_none());
    assert_eq!(no_bin.missing_artifacts, vec![Dimension::Bin]);
    for v in &no_bin.verdicts {
        match v.test_type {
            Dimension::Lib => assert!(v.passed(), "{}", v.name),
            Dimension::Bin => assert_eq!(v.cause(), Some(FailureCause::MissingArtifact), "{}", v.name),
        }
    }
    let usage = no_bin
        .verdicts
        .iter()
        .find(|v| v.name == "ToyBin.UsageNamesTheProgram")
        .unwrap();
    assert!(
        usage.failure.contains("harvest_missing/toy: No such file or directory"),
        "{}",
        usage.failure
    );

    // gen-manifest measures against every declared artifact and keeps the
    // authored program and test_types.
    let generated = work.join("generated_manifest.json");
    let _ = std::fs::remove_file(&generated);
    let status = Command::new(env!("CARGO_BIN_EXE_harvest-bench"))
        .args(["gen-manifest", "--suite"])
        .arg(&suite)
        .arg("--lib")
        .arg(&c_lib)
        .arg("--bin")
        .arg(&c_bin)
        .arg("--build-dir")
        .arg(work.join("gen_manifest"))
        .arg("--out")
        .arg(&generated)
        .status()
        .unwrap();
    assert!(status.success());
    let generated: serde_json::Value =
        serde_json::from_str(&std::fs::read_to_string(&generated).unwrap()).unwrap();
    let authored: serde_json::Value =
        serde_json::from_str(&std::fs::read_to_string(suite.join("manifest.json")).unwrap()).unwrap();
    assert_eq!(generated["program"], authored["program"]);
    assert_eq!(generated["test_types"], authored["test_types"]);
    assert_eq!(generated["invocations"].as_array().unwrap().len(), 12);
}
