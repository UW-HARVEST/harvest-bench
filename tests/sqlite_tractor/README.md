# SQLite TRACTOR

The DARPA SQLite engine and its `sqlite3` shell, tested with selected
same-version upstream SQLite regressions. The program has two dimensions:
the library `libsqlite3.so` and the shell executable. The suite has **1,278
gtests**: 1,188 library tests and 90 CLI tests. Each test has a fresh
interpreter, database and working directory.

## Sources and scope

- **Engine and shell**: Test-Corpus commit
  `cdbdcaac07d48b45e401d36ce68960d5b6ac1c58`,
  `Public-Tests/P03_sqlite/sqlite/test_case` (MIT corpus license; SQLite
  public-domain notices retained).
- **Tests**: SQLite commit `924626e603a36cc48ce87bc3a3eeddc61af1a72f` (Fossil
  `86bc7c8648fa6df303cd6953e377227d8a778b7becca984332bb6f623cb52cec`), a
  3.54.0 development snapshot. See `gtest_suite/upstream/README.md`.

The DARPA version differs from upstream SQLite:

- It removes WAL (`wal.c`), the mutex and thread layers (`mutex*.c`,
  `threads.c`, `btmutex.c`), shared cache and unlock-notify (`notify.c`),
  mmap, and the debug allocators (`mem2.c`, `mem3.c`). Locking modes and busy
  handling do not behave as upstream.
- It changes some public signatures and removes some constants (see the DARPA
  `sqlite3.h` in `gtest_suite/upstream/`).
- Its shell has no `.timeout`, `.imposter`, `.intck` or `.expert` command.
- Its Tcl loader `load_static_extension` needs an open connection command.

The tests are therefore upstream tests that do not depend on these parts:
expressions and functions, integer boundaries, JSON, queries, CTEs and
windows, DDL/DML, constraints, triggers and views, foreign keys, transactions
and savepoints, public C API wrappers, FTS3/4/5, R-Tree, session, recover,
and the shell. The library tests are 666 literal SQL batches from 38 files and
522 complete script sequences. The CLI tests come from `shell1.test` to
`shellB.test` and `recover.test`.

Excluded: WAL, snapshot, mutex, thread, shared-cache, locking and mmap tests;
tests of private testfixture APIs and internal FTS diagnostics; fault
injection and crash VFS; plugins and performance tests; GB-sized data
(`bigsort`, `bigfile`, `fts5bigpl`, `sessionbig`); two vacuum pressure
sequences; unselected parts of the 38 split files; `select1-15.1`..`15.3`
(locking); `shell1-3.25.1`..`3.25.4` (`.timeout`); `shellB`'s `imposter1.sql`
and `intck01.sql`; `ext/expert/expert1.test` (`.expert`, private
`sqlite3_expert_new`); and `avfs.test`, whose fixture stops at
`load_static_extension` after `db close` and which mostly tests the append
VFS through the library. No expected result was changed.

## Adaptations

**Build** (`test_case/CMakeLists.txt`; the C sources are unchanged): FTS3 on;
standalone dynamic extension targets and engine-side Tcl off; new option
`SQLITE_SHELL_ENABLE_RECOVER` (default ON) enables the shell's `.recover`.
The default build makes `libsqlite3.so` (target `sqlite3_shared`) and the
shell `driver` (target `sqlite3_cli`, engine linked statically).
`configuration.json` has no variables: one feature configuration is tested.

**Scenarios**: a literal SQL batch runs on an empty database and never
replays an earlier assertion. Other files run as complete sequences and score
once. Split CLI files run as the file's unchanged setup plus a group of
assertions; a group is separate only if it does not depend on state that the
earlier assertions left, and it fails with an executable that does nothing.
The scenario registry (`scenario_table.h`, `scenarios/`) is fixed, and each
scene must execute exactly the upstream assertion names in `traces/`.
Missing commands, setup errors and incomplete runs fail the scene.

**Determinism**: Tcl seed 1234; a delegating VFS fixes entropy (xorshift64
seed `0x4d595df4d0f33173`) and time (2000-01-01 00:00:00 UTC); locale C, UTF-8,
UTC. `fts5secure3` takes rowids from a test-side xorshift64 `random()`
(seed `0x9e3779b97f4a7c15`). `fuzzy.log` is discarded.

**ABI**: every library scene checks that the library exports all 368 public
symbols in `public_abi.h`, including symbols it does not call. The Tcl bridge
uses only the public API.

**CLI tests**: upstream scripts start the shell themselves with Tcl `exec`.
`glue/cli.h` gives each scene a private directory with a link named
`program.bin.name` to the executable (so `argv[0]` is that name), a private
`HOME` and `TMPDIR`, fixed `PATH`, `LC_ALL=C`, `TZ=UTC`, stdin `/dev/null`
and no core files. The Tcl side of a CLI scene uses the library under test,
as upstream testfixture does. `shellB`'s `do_clitest` trusts the test count
that the shell prints, so its traces also record the C reference's count and
the suite requires it.

Budgets are measured C times with a factor of 3 and a 10-second minimum; one
`case` entry covers all CLI scenes.

## Build and run

Requirements: Linux, C/C++17 compilers, CMake, zlib and Tcl 8.6 development
files, GoogleTest 1.17.0 (fetched at configure time).

```sh
cargo build --release
./target/release/harvest-bench verify tests/sqlite_tractor
./target/release/harvest-bench run --suite tests/sqlite_tractor/gtest_suite \
  --lib /abs/path/to/libsqlite3.so --bin /abs/path/to/sqlite3-shell
```

Without `--bin`, the CLI tests fail as `missing_artifact`. The suite links
the library at build time, so it needs `--lib`. With plain CMake:

```sh
cmake -S tests/sqlite_tractor/test_case -B tests/sqlite_tractor/build/c \
  -DCMAKE_BUILD_TYPE=Release
cmake --build tests/sqlite_tractor/build/c -j8
cmake -S tests/sqlite_tractor/gtest_suite -B tests/sqlite_tractor/build/gtest \
  -DCMAKE_BUILD_TYPE=Release \
  -DTEST_LIB_PATH="$PWD/tests/sqlite_tractor/build/c/libsqlite3.so" \
  -DTEST_BIN_PATH="$PWD/tests/sqlite_tractor/build/c/driver" \
  -DTEST_BIN_NAME=sqlite3
cmake --build tests/sqlite_tractor/build/gtest -j8
./tests/sqlite_tractor/build/gtest/harvest_gtest
```

## Mechanical Rust positive control

A C2Rust 0.22.1 translation of the engine and the shell, with no edits to the
generated Rust, passes all 1,278 tests with these budgets.

## Maintenance

The scenario registry, traces and `public_abi.h` are fixed data. A change to
them changes the denominator and needs a passing C replay of every changed
scene.
