#!/usr/bin/env python3
"""Reproduce this fixed-config C2Rust experiment in a NEW output directory.

Requires c2rust 0.22.1, Clang/CMake and installed nightly-2025-05-18. Does not
build or link SQLite C; CMake is used only to recover the compilation database.
Example:
  python3 tests/sqlite_tractor/tools/c2rust/reproduce.py --source tests/sqlite_tractor/test_case \
    --out /tmp/sqlite-c2rust-reproduced
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def run(argv, cwd, log):
    with log.open('w') as stream:
        subprocess.run(argv, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT,
                       timeout=600, check=True)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    args = p.parse_args()
    source, out = args.source.resolve(), args.out.resolve()
    if out.exists():
        p.error('--out must not exist; preserve earlier experiments')
    out.mkdir(parents=True)
    shutil.copytree(source, out / 'c_src')
    compat = out / 'compat/sys'
    compat.mkdir(parents=True)
    (compat / 'cdefs.h').write_text(
        '#include_next <sys/cdefs.h>\n#undef __glibc_const_generic\n')
    run(['cmake', '-S', str(out / 'c_src'), '-B', str(out / 'c_build'),
         '-DCMAKE_C_COMPILER=clang', '-DCMAKE_BUILD_TYPE=Release',
         '-DSQLITE_TEST=OFF', '-DSQLITE_BUILD_CLI=OFF',
         '-DSQLITE_BUILD_DYNAMIC_EXTENSIONS=OFF', '-DSQLITE_ENABLE_FTS3=ON',
         '-DCMAKE_C_FLAGS=-I' + str(out / 'compat')], out,
        out / 'configure.log')
    db = out / 'c_build/compile_commands.json'
    units = json.loads(db.read_text())
    assert len(units) == 130, 'unexpected fixed-config source inventory'
    shutil.copy2(db, out / 'compile_commands.json')
    run(['c2rust', 'transpile', str(db), '--emit-build-files',
         '--output-dir', str(out / 'rust'), '--disable-rustfmt'], out,
        out / 'transpile.log')
    rust = out / 'rust'
    shutil.copytree(rust, out / 'generated-pristine')
    manifest = rust / 'Cargo.toml'
    text = manifest.read_text().replace('name = "rust"',
                                       'name = "sqlite_tractor_c2rust"')
    text = text.replace('crate-type = ["staticlib", "rlib"]',
                        'crate-type = ["cdylib"]')
    text += '\n[profile.release]\noverflow-checks = false\n'
    manifest.write_text(text)
    (rust / 'rust-toolchain.toml').write_text(
        '[toolchain]\nchannel = "nightly-2025-05-18"\n')
    (rust / 'build.rs').write_text('fn main(){for lib in ["m","z","dl"] '
        '{println!("cargo:rustc-link-lib={}",lib);}}\n')
    shutil.copy2(Path(__file__).with_name('cargo-dependencies.lock'),
                 rust / 'Cargo.lock')
    run(['cargo', '+nightly-2025-05-18', 'build', '--release', '--locked'],
        rust, out / 'build.log')
    library = rust / 'target/release/libsqlite_tractor_c2rust.so'
    summary = {'translation_units': len(units), 'library': str(library),
               'sha256': hashlib.sha256(library.read_bytes()).hexdigest(),
               'toolchain': 'nightly-2025-05-18',
               'rust_engine_source_edits': 0}
    (out / 'build-summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
