//! Locating the artifacts of the reference C build through the CMake file
//! API (codemodel-v2): CMake records every target's type and output path, so
//! the runner neither guesses file names nor depends on output directories.

use std::fs;
use std::path::{Path, PathBuf};

use crate::{Artifacts, ProgramSpec, Result};

/// Writes the codemodel-v2 query. Must run before the configure step.
pub(crate) fn write_query(build_dir: &Path) -> Result<()> {
    let query = build_dir.join(".cmake/api/v1/query/codemodel-v2");
    fs::create_dir_all(query.parent().expect("has parent"))?;
    fs::write(&query, "")?;
    Ok(())
}

/// Locates the program's declared artifacts in a configured and built tree:
/// each declared `c_target`, or else the build's only target of the needed
/// type. Every declared artifact must be found.
pub(crate) fn locate_artifacts(build_dir: &Path, program: &ProgramSpec) -> Result<Artifacts> {
    let targets = read_targets(build_dir)?;
    let mut out = Artifacts::default();
    if let Some(lib) = &program.lib {
        out.lib = Some(pick(&targets, lib.c_target.as_deref(), "SHARED_LIBRARY")?);
    }
    if let Some(bin) = &program.bin {
        out.bin = Some(pick(&targets, bin.c_target.as_deref(), "EXECUTABLE")?);
    }
    Ok(out)
}

struct Target {
    name: String,
    /// CMake target type: EXECUTABLE, SHARED_LIBRARY, OBJECT_LIBRARY, ...
    kind: String,
    /// Absolute path of the target's first artifact (the binary itself).
    artifact: Option<PathBuf>,
}

fn pick(targets: &[Target], wanted: Option<&str>, ty: &str) -> Result<PathBuf> {
    let matches: Vec<&Target> = targets
        .iter()
        .filter(|t| match wanted {
            Some(name) => t.name == name,
            None => t.kind == ty,
        })
        .collect();
    match matches.as_slice() {
        [one] if one.kind != ty => Err(format!(
            "CMake target '{}' is a {}, expected a {}",
            one.name, one.kind, ty
        )
        .into()),
        [one] => one
            .artifact
            .clone()
            .ok_or_else(|| format!("CMake target '{}' has no artifact", one.name).into()),
        [] => Err(match wanted {
            Some(name) => format!("the test_case build has no target '{}'", name),
            None => format!("the test_case build has no {} target", ty),
        }
        .into()),
        many => Err(format!(
            "the test_case build has {} {} targets ({}); declare c_target in manifest.json",
            many.len(),
            ty,
            many.iter().map(|t| t.name.as_str()).collect::<Vec<_>>().join(", ")
        )
        .into()),
    }
}

/// Reads the build's targets from the file API reply.
fn read_targets(build_dir: &Path) -> Result<Vec<Target>> {
    let reply = build_dir.join(".cmake/api/v1/reply");
    let index = fs::read_dir(&reply)
        .map_err(|e| {
            format!(
                "no CMake file API reply in {} (CMake 3.14 or newer is required): {}",
                reply.display(),
                e
            )
        })?
        .filter_map(|e| e.ok())
        .map(|e| e.path())
        .find(|p| {
            p.file_name()
                .is_some_and(|n| n.to_string_lossy().starts_with("codemodel-v2-"))
        })
        .ok_or_else(|| format!("no codemodel-v2 reply in {}", reply.display()))?;
    let model: serde_json::Value = serde_json::from_str(&fs::read_to_string(&index)?)?;
    let config = model["configurations"]
        .as_array()
        .and_then(|c| c.first())
        .ok_or("the CMake codemodel has no configuration")?;
    let mut out = Vec::new();
    for t in config["targets"].as_array().into_iter().flatten() {
        let Some(file) = t["jsonFile"].as_str() else {
            continue;
        };
        let target: serde_json::Value = serde_json::from_str(&fs::read_to_string(reply.join(file))?)?;
        let artifact = target["artifacts"]
            .as_array()
            .and_then(|a| a.first())
            .and_then(|a| a["path"].as_str())
            .map(|p| {
                let p = PathBuf::from(p);
                if p.is_absolute() {
                    p
                } else {
                    build_dir.join(p)
                }
            });
        out.push(Target {
            name: target["name"].as_str().unwrap_or("").to_string(),
            kind: target["type"].as_str().unwrap_or("").to_string(),
            artifact,
        });
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A build tree with a hand-written file API reply: two shared libraries
    /// (`aplugin` sorts first) and one executable.
    fn fake_build(tag: &str) -> PathBuf {
        let dir = std::env::current_exe()
            .unwrap()
            .parent()
            .unwrap()
            .join(format!("cmake-api-{}-{}", tag, std::process::id()));
        let reply = dir.join(".cmake/api/v1/reply");
        fs::create_dir_all(&reply).unwrap();
        let targets = [
            ("aplugin", "SHARED_LIBRARY", "libaplugin.so"),
            ("toy", "SHARED_LIBRARY", "libtoy.so"),
            ("toyexe", "EXECUTABLE", "bin/driver"),
            ("objs", "OBJECT_LIBRARY", ""),
        ];
        let mut index = Vec::new();
        for (name, ty, path) in targets {
            let file = format!("target-{}.json", name);
            let artifacts = if path.is_empty() {
                serde_json::json!([])
            } else {
                serde_json::json!([{ "path": path }])
            };
            let body = serde_json::json!({ "name": name, "type": ty, "artifacts": artifacts });
            fs::write(reply.join(&file), body.to_string()).unwrap();
            index.push(serde_json::json!({ "name": name, "jsonFile": file }));
        }
        let model = serde_json::json!({ "configurations": [{ "targets": index }] });
        fs::write(reply.join("codemodel-v2-0.json"), model.to_string()).unwrap();
        dir
    }

    fn program(raw: &str) -> ProgramSpec {
        serde_json::from_str(raw).unwrap()
    }

    #[test]
    fn locates_declared_targets() {
        let dir = fake_build("ok");
        let a = locate_artifacts(
            &dir,
            &program(r#"{"lib":{"c_target":"toy"},"bin":{"name":"toy"}}"#),
        )
        .unwrap();
        assert_eq!(a.lib, Some(dir.join("libtoy.so")));
        assert_eq!(a.bin, Some(dir.join("bin/driver")), "the only executable");
        fs::remove_dir_all(&dir).unwrap();
    }

    #[test]
    fn rejects_ambiguous_missing_and_mistyped_targets() {
        let dir = fake_build("bad");
        let err = |raw: &str| locate_artifacts(&dir, &program(raw)).unwrap_err().to_string();
        assert!(err(r#"{"lib":{}}"#).contains("2 SHARED_LIBRARY targets (aplugin, toy)"));
        assert!(err(r#"{"lib":{"c_target":"nope"}}"#).contains("no target 'nope'"));
        assert!(err(r#"{"lib":{"c_target":"toyexe"}}"#).contains("is a EXECUTABLE, expected a SHARED_LIBRARY"));
        assert!(err(r#"{"bin":{"c_target":"objs","name":"x"}}"#).contains("expected a EXECUTABLE"));
        fs::remove_dir_all(&dir).unwrap();
    }

    #[test]
    fn missing_reply_is_an_error() {
        let dir = std::env::current_exe().unwrap().parent().unwrap().join("no-such-build-tree");
        let e = locate_artifacts(&dir, &ProgramSpec::library_only()).unwrap_err().to_string();
        assert!(e.contains("no CMake file API reply"), "{e}");
    }
}
