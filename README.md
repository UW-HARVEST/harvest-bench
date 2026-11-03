# Harvest Bench

Harvest Bench is a benchmark that validates C library reimplementations.
It contains real C libraries, their upstream test suites, and a runner that
scores a reimplementation against those tests.

We built the benchmark to check the quality of C-to-Rust translations. The
scope is wider than Rust. Library tests link against one shared library file
and call its exported C symbols. Executable tests run one executable file and
check its output, exit status, and the files it writes. Any implementation
that compiles to a shared library with the same ABI as the original C
library, or to an executable with the same behavior, can take the test. This
includes translations of C to other languages, reimplementations of C in C,
and equivalence checks between two implementations of the same API.

## Repository layout

```
tests/            One directory per test program
  lz4/
    README.md     Provenance, adaptation notes, and usage for this test
    test_case/    The C program source and its CMake build (some tests also
                  have upstream API documentation in test_case/doc/)
    gtest_suite/  The GoogleTest suite and its manifest.json
  jansson/
  mujs/
  pcre2/
  zstd/
  sqlite_tractor/
  ...
runner/           The runner: a Rust library and the harvest-bench CLI
  support/        harvest_bin.h, the helper that executable tests use
  fixtures/       Small programs that the runner's own regression test uses
doc/              Detailed documentation for suite authors
```

Each test directory is self-contained. The suite fetches its own pinned
GoogleTest at configure time and needs no files outside its directory.

## Where the tests come from

Each suite uses the test suite of the upstream C project when the project has
one. When the upstream project has no usable suite, the test ports vectors
from similar sources instead. The README inside each test directory states
the exact provenance and what was excluded.

We changed the upstream tests as little as possible. The largest change is a
split of monolithic test programs into many small test cases. The split runs
the upstream program once, records its progress, and parses the record into
one verdict per scenario. The test logic stays identical to upstream. Fixed
seeds replace time-based seeds, so every run is deterministic.

## How to run the tests

Every suite is a standard GoogleTest program. You can build and run it with
CMake and nothing else. The runner adds three things on top: per-test process
isolation, time budgets that catch pathological slowness, and batched
invocations that keep large suites fast.

### With the runner

Build the runner once:

```bash
cargo build --release
export PATH="$PWD/target/release:$PATH"
```

To check that a test passes against its reference C library:

```bash
harvest-bench verify tests/lz4
```

To check every test in the repository:

```bash
harvest-bench verify tests/
```

To score your own implementation, point the runner at your shared library:

```bash
harvest-bench run --suite tests/lz4/gtest_suite --lib /path/to/your/liblz4.so
```

For a program that also has an executable, give it with `--bin`:

```bash
harvest-bench run --suite tests/<name>/gtest_suite \
      --lib /path/to/your/libfoo.so --bin /path/to/your/foo
```

The runner reads `gtest_suite/manifest.json` and turns it into process-level
invocations. A `case` entry runs each matched test in its own process, so a
crash in your library fails one test and not the whole run. A `suite` entry
runs a group of tests in one process, which keeps suites with thousands of
small cases fast. Each entry carries a time budget measured against the
reference C library. A test that exceeds its budget fails, because slowness
far beyond the C baseline is a defect. Add `--json report.json` to get a
machine-readable report.

### With plain GoogleTest

Each suite accepts the CMake variable `TEST_LIB_PATH`, with the absolute
path of the library under test. A suite for a program with an executable
also accepts `TEST_BIN_PATH` and `TEST_BIN_NAME` (see
[doc/programs.md](doc/programs.md)). For a library-only suite:

```bash
cd tests/lz4
cmake -S test_case -B build/c -DCMAKE_BUILD_TYPE=Release
cmake --build build/c -j
cmake -S gtest_suite -B build/gtest -DCMAKE_BUILD_TYPE=Release \
      -DTEST_LIB_PATH=$PWD/build/c/liblz4.so
cmake --build build/gtest -j
LD_LIBRARY_PATH=$PWD/build/c ./build/gtest/harvest_gtest
```

All GoogleTest flags work as usual, for example `--gtest_filter` and
`--gtest_list_tests`. This path loses the runner's extras: no crash
isolation between tests, no time budgets, and no batching. A hard crash in
the library under test ends the whole run and GoogleTest writes no report.

## Programs with an executable

A test program can have a shared library, an executable, or both. For
example, SQLite produces `libsqlite3.so` and the `sqlite3` shell, which links
the same engine. The suite's `manifest.json` declares the artifacts in a
`program` block and marks each test as a library test or an executable test.
A manifest without a `program` block declares a library-only program.
Executable tests use the helper `runner/support/harvest_bin.h`, which runs
the executable in its own directory with a timeout.

- [doc/programs.md](doc/programs.md): the `program` block, test types, and
  what happens when an artifact is missing.
- [doc/executable-tests.md](doc/executable-tests.md): how to write executable
  tests with the helper, and the example in `runner/fixtures/`.

## Requirements

- CMake 3.14 or newer, a C11 compiler, and a C++17 compiler for the suites.
- Network access on the first configure, to fetch the pinned GoogleTest. For
  offline builds, pre-populate the source once and pass
  `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest-1.17.0`.
- Rust 1.99 for the runner (pinned in `rust-toolchain.toml`).
- Tcl 8.6 runtime and development headers for `sqlite_tractor`.
- Linux. The aggregate suites and `harvest_bin.h` use `fork`, and the runner
  assumes ELF shared libraries.

## Notes for suite authors

To add a test program:

1. Put the C program and its CMake build under `tests/<name>/test_case/`.
2. Port the upstream test suite into `tests/<name>/gtest_suite/` with a
   `harvest_gtest` executable target that links `TEST_LIB_PATH`. For a
   program with an executable, also see
   [Programs with an executable](#programs-with-an-executable).
3. Run `harvest-bench gen-manifest` against the C build to measure baseline
   budgets, then group aggregate suites into `suite` entries by hand.
4. Run `harvest-bench verify tests/<name>` and make sure every test passes.
5. Document provenance, exclusions, and adaptations in `tests/<name>/README.md`.
