# SQLite TRACTOR

This benchmark uses the DARPA SQLite engine and selected same-version upstream
regressions. The fixed suite has **1,188 independently replayable gtests**:
666 literal SQL batches and 522 bounded stateful script sequences. Each has a
fresh interpreter, database and working directory. A sequence may contain many
upstream assertions; it contributes one verdict. Its assertions do not claim
independent setup or separate scores.

## Sources and scope

The engine comes from Test-Corpus commit
`cdbdcaac07d48b45e401d36ce68960d5b6ac1c58`,
`Public-Tests/P03_sqlite/sqlite/test_case`. The tests come from SQLite commit
`924626e603a36cc48ce87bc3a3eeddc61af1a72f`, Fossil check-in
`86bc7c8648fa6df303cd6953e377227d8a778b7becca984332bb6f623cb52cec`.
This is a 3.54.0 development snapshot, not a release interchangeable with
other snapshots carrying that version string.

The DARPA engine is the oracle. Its C and public header files are unchanged.
Build adaptation changes four CMake defaults: enable FTS3, disable the CLI,
disable standalone dynamic extension targets, and disable engine-side Tcl.
`configuration.json` has no configurable variables: this benchmark tests one
feature configuration. The corpus MIT license and SQLite public-domain notices
are retained. File hashes are in `source_integrity.json` and
`gtest_suite/upstream_integrity.json`.

The Release configuration retains FTS3/4, FTS5, R-Tree/Geopoly, session/preupdate,
JSON/JSONB, math/percentile, carray, dbpage/dbstat and the original common
extensions. Column metadata, UPDATE/DELETE LIMIT, MEMSYS5 and scanstatus remain
off. The reference shared library has no Tcl dependency. It does not restore
DARPA's removed WAL, locking, mutex, threading, shared-cache or mmap behavior.

Coverage includes expressions/functions, integer boundaries, JSON, queries,
CTEs/windows, DDL/DML, constraints, triggers/views, foreign keys, transactions,
savepoints, public C API wrappers, FTS3/4/5, R-Tree, session and recover. The
fixed 5,000-iteration `fuzz` regression, `rtree4`, `savepoint6`, all four integer
boundary files, and ordinary-size `bigrow` cases are retained without reducing
their loops or changing expected results.

This edition does not cover all upstream SQLite tests. Exclusions include
fault injection, private testfixture APIs, internal FTS diagnostics, concurrency,
CLI/plugin programs, timing benchmarks, GB boundary allocations and unselected
parts of split source files. Two vacuum pressure sequences are excluded after initial resource screening. The three recover sequences use public recovery
APIs. Known DARPA failures, including `$::str` SQL parameter syntax, are never
accepted by changing upstream expected results.

`gtest_suite/scenario_manifest.json` records every scenario's source, original
ID/range, setup, seed, transformations and comparison rule.
`source_selection.json` gives all 1,469 audited leaf scripts' dispositions.
The 666 literal batches are selected complete SQL commands that pass in an
empty database; they never replay earlier assertions as setup. Other script
sequences retain their entire original setup and are deliberately scored as
one unit. Four identical SQL batches are represented only once.

## Build and run

Requirements: Linux, C/C++17 compilers, CMake, zlib development files, Tcl 8.6
runtime/development files, and GoogleTest 1.17.0 (fetched at configure time).
The acceptance environment uses Tcl 8.6.17, GCC 15.2 and CMake 4.2.

From the harvest-bench root:

```sh
cargo build --release
./target/release/harvest-bench verify tests/sqlite_tractor
./target/release/harvest-bench run \
  --suite tests/sqlite_tractor/gtest_suite --lib /absolute/path/to/candidate.so
```

The suite is self-contained. Its only required library input is `TEST_LIB_PATH`:

```sh
cmake -S tests/sqlite_tractor/test_case -B tests/sqlite_tractor/build/c \
  -DCMAKE_BUILD_TYPE=Release
cmake --build tests/sqlite_tractor/build/c -j8
cmake -S tests/sqlite_tractor/gtest_suite -B tests/sqlite_tractor/build/gtest \
  -DCMAKE_BUILD_TYPE=Release \
  -DTEST_LIB_PATH="$PWD/tests/sqlite_tractor/build/c/libsqlite3.so"
cmake --build tests/sqlite_tractor/build/gtest -j8
./tests/sqlite_tractor/build/gtest/harvest_gtest --gtest_list_tests
```

For offline builds pass
`-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest-1.17.0`.
No research checkout, audit files, official engine, system SQLite or prebuilt
bridge is required. See `gtest_suite/upstream/README.md` for bridge provenance.

## Environment, ABI and verdicts

Every scenario uses Tcl seed 1234, retaining upstream explicit reseeding.
A delegating VFS fixes entropy with xorshift64 seed `0x4d595df4d0f33173` and
fixes time at 2000-01-01 00:00:00 UTC. Every native VFS callback receives the
original VFS object. SQLite PRNG initialization is reset at scenario startup.
Locale is C, Tcl encoding is explicitly UTF-8, and timezone is UTC.
`fuzzy.log` is discarded; generated SQL and all loop iterations are retained.
The engine's length limits are unchanged.
For `fts5secure3`, rowid inputs use a separate test-side signed xorshift64
generator (seed `0x9e3779b97f4a7c15`) registered as SQL `random()`. This
keeps rowid-bearing assertion names independent of the candidate's internal
random-byte consumption; the upstream SQL, loops and comparisons are intact.

The committed registry is independent of the candidate. Capability choices
are fixed, and the exact sequence of executed upstream assertion names is
checked. Missing commands, empty execution, missing completion, setup failures
and assertion mismatches fail the scene. Caught missing functional commands
also fail it. The optional FTS5 corruption marker uses its original Release
implementation, which has no engine interaction. Upstream result, error,
floating-point, NULL and regular-expression comparisons remain intact.

`public_abi.json` classifies the fixed C exports into 368 required public
symbols and internal implementation exports, and records declarations absent
from this fixed build. It uses DARPA public headers, selected extension headers
and two public static-extension initializers declared by DARPA's Tcl loader.
Every scenario checks the whole public ABI, including symbols it does not call.
The bridge imports only public SQLite APIs and treats engine objects as opaque.

All manifest entries use `case` mode. A crash or timeout fails only that process.
`validation.json` records three fresh-process rounds in original, reverse and
fixed shuffled order, final C timings, RSS, sampled aggregate file peaks and
runner isolation checks. The selection bounds are 128 MiB of generated files
and 512 MiB RSS per C scene; file peaks include writable open/unlinked SQLite temp files and are sampled every 2 ms and are measured
observations, not exact write-by-write upper bounds. No binary fixtures are
shipped. Budgets use the measured final C invocation time, a factor of 3 and a
10-second minimum.

## Mechanical Rust positive control

The installed C2Rust 0.22.1 translated all 130 library translation units.
The resulting Rust cdylib passes all 1,188 scenarios with the original budgets,
exports the full public ABI, and has no C SQLite/system SQLite dependency.
Generated Rust engine modules were not manually edited. Build adaptation uses
a glibc header overlay, platform library links and the compatible installed
`nightly-2025-05-18` toolchain. A fresh-directory reproduction produced the
same library bytes. This is mechanical unsafe Rust, not a safe-Rust rewrite.
See [mechanical-rust-validation.json](mechanical-rust-validation.json).

To reproduce the optional control (the benchmark itself does not require
C2Rust), from this directory:

```sh
python3 tools/c2rust/reproduce.py --source test_case --out /tmp/sqlite-c2rust
```

## Maintenance

After reviewing registry or ABI changes, run `gtest_suite/tools/sync_tables.py`.
`glue/abi_audit.py TEST_CASE REFERENCE_SO OUTPUT_JSON` regenerates the ABI
classification with Clang and nm. `tools/extract_literal_batches.tcl` extracts
literal upstream commands for review; extraction does not admit a scenario.
Its complete SQL setup, unchanged expected value, scope and C replay must be
checked before adding it to the fixed manifest.

`tools/validate.py --binary /path/to/harvest_gtest --output validation.json`
runs the three resource/determinism rounds. The validation-only `fault_shim.c`
and `isolation.rs` exercise the runner's real case-mode plan execution with
setup, assertion, signal and timeout failures; they are never linked into the
suite or reference library.
