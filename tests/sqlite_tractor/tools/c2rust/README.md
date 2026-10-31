# Optional mechanical Rust positive control

`reproduce.py` translates the benchmark's fixed C configuration with C2Rust,
then builds the generated Rust engine as a shared library. This is an optional
validation recipe, separate from the scoring suite. It never modifies the C
source, external tests, expected outputs or budgets.

The verified environment used C2Rust 0.22.1, Clang 21.1.8 and installed
`nightly-2025-05-18` (rustc 1.89.0-nightly). The older nightly supports the
historical variadic APIs emitted by C2Rust. `cargo-dependencies.lock` pins Rust
dependencies. The header overlay only disables glibc qualifier-generic macros
unsupported by C2Rust; it preserves underlying libc functions and feature-test
macro timing. Cargo links platform libc/math/zlib/dl, with no SQLite C engine.

From the harvest-bench root, choose a new output directory:

```sh
python3 tests/sqlite_tractor/tools/c2rust/reproduce.py \
  --source tests/sqlite_tractor/test_case --out /tmp/sqlite-tractor-c2rust
./target/release/harvest-bench run \
  --suite tests/sqlite_tractor/gtest_suite \
  --lib /tmp/sqlite-tractor-c2rust/rust/target/release/libsqlite_tractor_c2rust.so \
  --build-dir /tmp/sqlite-tractor-c2rust/gtest_build \
  --json /tmp/sqlite-tractor-c2rust/results.json
```

The recorded experiment translated all 130 compilation units without editing
any generated engine Rust module. It exported all 368 required public symbols
and passed 1,188/1,188 original cases with zero failures/skips. A fresh execution
of this recipe regenerated all 130 Rust modules and the final shared library
byte-for-byte. This establishes a Rust implementation accepted by this suite;
it does not establish memory safety or idiomatic Rust quality.
