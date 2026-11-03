//! The program under test: which artifacts a suite tests (a shared library,
//! an executable, or both), how the reference C build produces them, and
//! how the artifacts of one run are checked against that declaration.
//!
//! The declaration is the optional `program` block of `manifest.json`:
//!
//! ```json
//! "program": {
//!   "lib": { "c_target": "toy" },
//!   "bin": { "c_target": "toyexe", "name": "toy" }
//! }
//! ```
//!
//! A present sub-block is a dimension of the program. A manifest without a
//! `program` block declares a library-only program, the historical contract.

use std::path::{Path, PathBuf};

use crate::Result;

/// A dimension of the program under test: its shared library or its
/// executable. It is also the type of a test: what the test exercises.
#[derive(serde::Deserialize, serde::Serialize, Clone, Copy, PartialEq, Eq, Debug, Default)]
#[serde(rename_all = "lowercase")]
pub enum Dimension {
    /// The shared library; its tests call the C API in-process.
    #[default]
    Lib,
    /// The executable; its tests run it as a child process.
    Bin,
}

impl Dimension {
    /// Stable lowercase label (`lib`, `bin`), as in the manifest and reports.
    pub fn label(self) -> &'static str {
        match self {
            Dimension::Lib => "lib",
            Dimension::Bin => "bin",
        }
    }

    /// The suite's CMake variable that receives this artifact's path.
    pub fn cmake_var(self) -> &'static str {
        match self {
            Dimension::Lib => "TEST_LIB_PATH",
            Dimension::Bin => "TEST_BIN_PATH",
        }
    }
}

impl std::fmt::Display for Dimension {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(self.label())
    }
}

/// The `program.lib` block.
#[derive(serde::Deserialize, serde::Serialize, Clone, Debug, Default, PartialEq)]
#[serde(deny_unknown_fields)]
pub struct LibSpec {
    /// The SHARED_LIBRARY target of `test_case/CMakeLists.txt`. Omitted: the
    /// reference build's only shared library target.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub c_target: Option<String>,
}

/// The `program.bin` block.
#[derive(serde::Deserialize, serde::Serialize, Clone, Debug, PartialEq)]
#[serde(deny_unknown_fields)]
pub struct BinSpec {
    /// The EXECUTABLE target of `test_case/CMakeLists.txt`. Omitted: the
    /// reference build's only executable target.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub c_target: Option<String>,
    /// The program name. Tests pass it as `argv[0]`, so output that prints
    /// the program name does not depend on the artifact's file name; a
    /// translation names its executable after it.
    pub name: String,
}

/// The `program` block of a manifest.
#[derive(serde::Deserialize, serde::Serialize, Clone, Debug, PartialEq)]
#[serde(deny_unknown_fields)]
pub struct ProgramSpec {
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub lib: Option<LibSpec>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub bin: Option<BinSpec>,
}

impl ProgramSpec {
    /// The program of a manifest without a `program` block.
    pub fn library_only() -> Self {
        ProgramSpec {
            lib: Some(LibSpec::default()),
            bin: None,
        }
    }

    pub fn has(&self, dim: Dimension) -> bool {
        match dim {
            Dimension::Lib => self.lib.is_some(),
            Dimension::Bin => self.bin.is_some(),
        }
    }

    /// Type of a test that matches no `test_types` entry: `lib` when the
    /// program has a library, else `bin`.
    pub fn default_test_type(&self) -> Dimension {
        if self.lib.is_some() {
            Dimension::Lib
        } else {
            Dimension::Bin
        }
    }

    pub(crate) fn validate(&self) -> Result<()> {
        if self.lib.is_none() && self.bin.is_none() {
            return Err("program declares neither lib nor bin".into());
        }
        if let Some(bin) = &self.bin {
            if bin.name.is_empty() || bin.name.contains('/') {
                return Err(format!(
                    "program.bin.name must be a plain file name, not '{}'",
                    bin.name
                )
                .into());
            }
        }
        Ok(())
    }
}

/// One `test_types` entry: the tests matching `filter` exercise `type`.
#[derive(serde::Deserialize, serde::Serialize, Clone, Debug)]
#[serde(deny_unknown_fields)]
pub struct TestTypeSpec {
    #[serde(rename = "type")]
    pub test_type: Dimension,
    /// Positive gtest filter patterns (`:`-separated, `*` and `?`).
    pub filter: String,
}

impl TestTypeSpec {
    pub(crate) fn validate(&self) -> Result<()> {
        let patterns: Vec<&str> = self.filter.split(':').collect();
        if patterns.iter().all(|p| p.is_empty()) {
            return Err("a test_types filter is empty".into());
        }
        if patterns.iter().any(|p| p.starts_with('-')) {
            return Err(format!(
                "test_types filter '{}' has a negative pattern; only positive patterns are supported",
                self.filter
            )
            .into());
        }
        Ok(())
    }
}

// ---------------------------------------------------------------------------
// Artifacts of one run

/// The artifacts given to one run. Each one reaches the suite through its
/// own CMake variable (see [`Dimension::cmake_var`]).
#[derive(Debug, Clone, Default, PartialEq, serde::Serialize)]
pub struct Artifacts {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub lib: Option<PathBuf>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub bin: Option<PathBuf>,
}

impl Artifacts {
    /// The artifacts of a library-only run.
    pub fn lib(path: impl Into<PathBuf>) -> Self {
        Artifacts {
            lib: Some(path.into()),
            bin: None,
        }
    }

    pub fn get(&self, dim: Dimension) -> Option<&PathBuf> {
        match dim {
            Dimension::Lib => self.lib.as_ref(),
            Dimension::Bin => self.bin.as_ref(),
        }
    }

    fn set(&mut self, dim: Dimension, path: PathBuf) {
        match dim {
            Dimension::Lib => self.lib = Some(path),
            Dimension::Bin => self.bin = Some(path),
        }
    }
}

/// The artifacts of a run after checking them against the program.
#[derive(Debug, Clone)]
pub struct ResolvedArtifacts {
    /// The given artifacts, canonicalized.
    pub given: Artifacts,
    /// Declared dimensions the run was not given an artifact for. Only the
    /// tests of these dimensions fail.
    pub missing: Vec<Dimension>,
}

impl ResolvedArtifacts {
    /// What the suite's CMake variables receive: the given artifacts, and
    /// for each missing one a path under `dir` that does not exist. The
    /// executable placeholder keeps the declared name, so exec errors name
    /// it.
    pub fn suite_paths(&self, program: &ProgramSpec, dir: &Path) -> Result<Artifacts> {
        let mut paths = self.given.clone();
        let dir = std::path::absolute(dir)?.join("harvest_missing");
        for &dim in &self.missing {
            let path = match dim {
                Dimension::Lib => dir.join("libharvest_missing.so"),
                Dimension::Bin => dir.join(&program.bin.as_ref().expect("declared").name),
            };
            if path.exists() {
                return Err(format!("placeholder path {} exists; remove it", path.display()).into());
            }
            paths.set(dim, path);
        }
        Ok(paths)
    }

    /// Shared-library search path for the suite process: the directory of
    /// the given library, if any.
    pub fn ld_library_path(&self) -> String {
        self.given
            .lib
            .as_ref()
            .and_then(|l| l.parent())
            .map(|p| p.to_string_lossy().into_owned())
            .unwrap_or_default()
    }
}

/// Checks a run's artifacts against the program.
///
/// - An artifact the program does not declare is an error.
/// - A given artifact must exist.
/// - When the program has both dimensions and the run gives only one
///   artifact, the other is missing (see [`ResolvedArtifacts::suite_paths`]).
///   When `allow_missing` is false, or no artifact is given at all, a missing
///   artifact is an error.
pub fn resolve_artifacts(
    artifacts: &Artifacts,
    program: &ProgramSpec,
    allow_missing: bool,
) -> Result<ResolvedArtifacts> {
    let mut given = Artifacts::default();
    let mut missing = Vec::new();
    let any_given = artifacts.lib.is_some() || artifacts.bin.is_some();

    for dim in [Dimension::Lib, Dimension::Bin] {
        match (artifacts.get(dim), program.has(dim)) {
            (Some(_), false) => {
                return Err(format!("the suite's program has no {} artifact, but one was given", dim).into())
            }
            (None, false) => {}
            (Some(p), true) => {
                let p = p
                    .canonicalize()
                    .map_err(|e| format!("{} artifact not found at {}: {}", dim, p.display(), e))?;
                given.set(dim, p);
            }
            (None, true) => {
                if !allow_missing || !any_given {
                    return Err(format!("the suite's program needs a {} artifact", dim).into());
                }
                missing.push(dim);
            }
        }
    }
    Ok(ResolvedArtifacts { given, missing })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn program(raw: &str) -> Result<ProgramSpec> {
        let p: ProgramSpec = serde_json::from_str(raw)?;
        p.validate()?;
        Ok(p)
    }

    #[test]
    fn program_blocks() {
        let both = program(r#"{"lib":{"c_target":"toy"},"bin":{"name":"toy"}}"#).unwrap();
        assert!(both.has(Dimension::Lib) && both.has(Dimension::Bin));
        assert_eq!(both.default_test_type(), Dimension::Lib);
        let bin = program(r#"{"bin":{"name":"toy"}}"#).unwrap();
        assert!(!bin.has(Dimension::Lib));
        assert_eq!(bin.default_test_type(), Dimension::Bin);
        assert_eq!(ProgramSpec::library_only().default_test_type(), Dimension::Lib);
    }

    #[test]
    fn program_rejects() {
        assert!(program(r#"{}"#).is_err(), "neither dimension");
        assert!(program(r#"{"bin":{}}"#).is_err(), "bin without name");
        assert!(program(r#"{"bin":{"name":"a/b"}}"#).is_err(), "bin name with a slash");
        assert!(program(r#"{"bin":{"name":""}}"#).is_err(), "empty bin name");
        assert!(program(r#"{"lib":{"name":"x"}}"#).is_err(), "unknown lib field");
        assert!(program(r#"{"kind":"lib"}"#).is_err(), "unknown program field");
    }

    #[test]
    fn test_type_filters() {
        let spec = |filter: &str| TestTypeSpec {
            test_type: Dimension::Bin,
            filter: filter.to_string(),
        };
        assert!(spec("Shell*:Cli.*").validate().is_ok());
        assert!(spec("").validate().is_err());
        assert!(spec(":").validate().is_err());
        assert!(spec("-Shell*").validate().is_err());
        assert!(spec("Shell*:-Shell.Slow").validate().is_err());
    }

    #[test]
    fn resolve_rules() {
        let exe = std::env::current_exe().unwrap();
        // Placeholders are only computed, never created: a directory that
        // does not exist keeps the test hermetic.
        let dir = exe
            .parent()
            .unwrap()
            .join(format!("resolve-rules-{}", std::process::id()));
        let lib_only = ProgramSpec::library_only();
        let both = program(r#"{"lib":{},"bin":{"name":"toy"}}"#).unwrap();
        let bin_only = program(r#"{"bin":{"name":"toy"}}"#).unwrap();
        let lib = Artifacts::lib(&exe);
        let bin = Artifacts { lib: None, bin: Some(exe.clone()) };
        let full = Artifacts { lib: Some(exe.clone()), bin: Some(exe.clone()) };

        assert!(resolve_artifacts(&lib, &lib_only, true).unwrap().missing.is_empty());
        assert!(resolve_artifacts(&full, &lib_only, true).is_err(), "undeclared bin");
        assert!(resolve_artifacts(&lib, &bin_only, true).is_err(), "undeclared lib");
        assert!(resolve_artifacts(&Artifacts::default(), &both, true).is_err(), "nothing given");
        assert!(resolve_artifacts(&Artifacts::lib(dir.join("x.so")), &lib_only, true).is_err());

        let r = resolve_artifacts(&lib, &both, true).unwrap();
        assert_eq!(r.missing, vec![Dimension::Bin]);
        assert!(r.given.bin.is_none());
        assert_eq!(r.given.lib, Some(exe.canonicalize().unwrap()));
        let paths = r.suite_paths(&both, &dir).unwrap();
        assert_eq!(paths.bin, Some(dir.join("harvest_missing/toy")));
        assert_eq!(paths.lib, r.given.lib);

        // A bin-only run of a program with both: the library is missing.
        let r = resolve_artifacts(&bin, &both, true).unwrap();
        assert_eq!(r.missing, vec![Dimension::Lib]);
        let paths = r.suite_paths(&both, &dir).unwrap();
        assert_eq!(paths.lib, Some(dir.join("harvest_missing/libharvest_missing.so")));
        assert!(r.ld_library_path().is_empty());
        assert!(resolve_artifacts(&lib, &both, false).is_err(), "strict mode");
    }
}
