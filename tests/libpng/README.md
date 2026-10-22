# libpng Library Test Case

## Source

- **Project**: [libpng](https://github.com/pnggroup/libpng) — the official PNG reference library
- **Version**: libpng16 branch
- **License**: PNG Reference Library License (libpng license, permissive)
- **Dependency**: libpng calls **zlib** for DEFLATE (it does not implement compression itself)

## What This Tests

The complete libpng library for C-to-Rust translation. `test_case/` contains
**only** verbatim upstream library sources and upstream API documentation
(`doc/`). There is no dispatch wrapper and no
other adaptation code in the translation input. The translated Rust cdylib must
export the **full public C ABI** of the library (`png_create_read_struct`,
`png_read_info`, `png_read_image`, `png_create_write_struct`, `png_write_*`,
`png_set_*` transform setup, `png_get_*`, ...).

**zlib dependency.** libpng does not implement DEFLATE — it calls zlib to
(de)compress the image `IDAT` stream. The C reference build links the system
zlib. A translated implementation is expected to provide the compression
itself (e.g. a pure-Rust zlib such as `flate2`/`miniz_oxide` bundled into the
cdylib), so it needs no external zlib at test time.

## Source Files

Verbatim upstream libpng library sources (15 `.c` + headers). The test driver
`pngtest.c` and the doc sample `example.c` are **excluded** — they are not part
of the library. `include/pnglibconf.h` is the prebuilt config header shipped
upstream in `scripts/pnglibconf.h.prebuilt` (normally produced at configure
time). The only non-upstream file is `test_case/CMakeLists.txt`, which builds
the sources into one shared library (`libpng.so`, linked against zlib).

## API Documentation: `test_case/doc/`

Upstream API documentation for the agents that translate and verify this
library. `png.h` does not state some API contracts, for example the legal bit
depths for each color type or the default user limits.

- **Source**: libpng commit `cae4745` (branch `libpng16`, `git describe`
  v1.6.58-9-gcae4745). This is the commit whose library files are identical
  to `test_case/`.
- **Files**: `doc/libpng-manual.txt`, verbatim. The license is the same as for
  the library (`test_case/LICENSE`).
- **Not included**: `libpng.3` (the same text as the manual), `png.5`, and
  everything under `contrib/`. `contrib/libtests/pngvalid.c` and the PNG test
  images are part of the test oracle.

## Tests: `gtest_suite/`

Validation wraps libpng's own correctness validator, **`pngvalid`**
(`contrib/libtests/pngvalid.c`, ~12k lines, verbatim). pngvalid self-generates
PNG images across bit depths, colour types, and interlacing, writes and reads
them back through the public API, and checks the round-tripped pixels against
computed expectations with tight numeric tolerances. It needs no external
files. The suite exposes **6,521 test cases**, one per pngvalid scenario.

`glue/glue_pngvalid.c` includes `pngvalid.c` verbatim (with `main` renamed and
`PNG_FREESTANDING_TESTS` defined so it resolves `<png.h>`/`<zlib.h>`/
`<cexcept.h>` from the include path rather than upstream-tree relative paths).

### Scenario-level reporting for the four test groups

pngvalid organizes its work into four groups, and each group is one loop over
thousands of independent scenarios that stops at the first failure. One
GoogleTest case per group would collapse each of those loops into a single
pass/fail bit, so every scenario is exposed as its own test case instead:

| gtest suite | pngvalid group | Scenarios | Covers |
|---|---|---|---|
| `LibpngStandard` | `--standard`  | 33 | baseline read/write over all bit depths, colour types and interlacing, plus the interlace-macro, formatting and error-handling checks |
| `LibpngSize`     | `--size`      | 5,068 | 1–16 pixel width/height grid against both readers and both interlace handlers |
| `LibpngTransform`| `--transform` | 611 | the read/write transform pipeline (expand, strip, gray↔rgb, ...) per format |
| `LibpngGamma`    | `--gamma`     | 809 | gamma correction: threshold, transform, sBIT, 16-to-8, background and alpha-mode sub-modes |
| **Total** | | **6,521** | |

`glue/glue_pngvalid.c` reproduces each group's loop structure and calls the
upstream scenario functions (`standard_test`, `transform_test`, `gamma_test`)
directly. The upstream file is not modified — including the source puts its
static functions in scope. ONE walk per group serves both enumeration and
execution, so the order gtest registers scenarios in and the order the
recording run visits them cannot drift apart. Two things differ from upstream
on purpose:

- the `if (fail(pm)) return;` early exits are dropped, so one failing scenario
  no longer hides every later one (upstream itself continues this way under
  its `-l` and `-v` options);
- each scenario's error counters are cleared before it runs, which turns
  pngvalid's run-wide error tally into a per-scenario verdict and leaves that
  scenario's own first message in `ps->error`.

Loop bounds, parameter sets, ordering, and per-group setup are upstream's. The
transcription is checked against upstream by construction: the scenario counts
above equal the number of `standard_test`/`transform_test`/`gamma_test` calls a
plain `pngvalid --standard|--size|--transform|--gamma` run makes, and each
group takes the same wall-clock time as that run.

Each group executes only once per library under test:

- The run happens in a **forked child** that streams one line per scenario
  through a pipe. A crashing or hanging library therefore still leaves a usable
  partial record, and because the parent survives, scenarios that already
  succeeded are still reported as passing.
- The child's deadline is the harness-provided soft timeout for the current
  invocation, computed from `manifest.json` and exported as
  `HARVEST_TEST_SOFT_TIMEOUT_SECS`, so the partial record arrives before the
  kill. `manifest.json` is the single authority on time.
- The record lives in process-local memory, so everything sharing the process
  shares one recording run. `manifest.json` batches each group into one
  `suite`-mode process invocation.
- The fork also contains the walk's side effects. Setting up a `png_modifier`
  advances pngvalid's global pseudo-random stream, so doing it in the parent
  would leave the recording run unable to reproduce a plain pngvalid run bit
  for bit.

### Where the test names come from

`glue/scenario_list.h` is a **checked-in** table of the 6,521 scenario names,
and registering the tests is a walk over that table. Registration therefore
executes no pngvalid code and does not touch the library under test at all.

The alternative — enumerating at startup — would make the test set depend on
something succeeding at run time. A library broken enough to break enumeration
would leave the suite reporting zero tests, which the harness reads as broken
infrastructure rather than as a score of zero. With a static list every
scenario is always registered, and a broken library simply fails all of them.

The list is generated from the same walk that runs the scenarios, so the names
and the run cannot drift apart, and a list upstream has outgrown is caught
rather than tolerated: the recording run reports how many scenarios it visited,
and every test of the group fails with an explicit "scenario_list.h is stale"
message if that disagrees with the table. Regenerate it after any change to
`upstream/pngvalid.c` or to the walk:

```bash
cmake --build build/gtest --target harvest_scenario_list
./build/gtest/harvest_scenario_list > gtest_suite/glue/scenario_list.h
```

Verdicts: a scenario that ran without raising a libpng error or warning passed,
one that raised either failed, and one the recording run never reached is
reported as a **failure, not a skip**. Skipped tests leave the harness's
denominator, which would let an early crash score better than a late one.

### Notes

- **Standard-gtest compatibility**: `manifest.json` is harness-only metadata,
  and the suite binary never reads it. A bare run, `--gtest_filter` and ctest
  stay correct. Filtering down to one scenario pays for one recording run of
  its group (0.2–1.8 s), so interactive debugging is cheap.
- **One gtest suite per group.** Each group is a value-parameterized suite
  (`TEST_P` + `INSTANTIATE_TEST_SUITE_P`) whose parameter is the scenario
  index, so all 6,521 cases live in four suites rather than at the top level.
  The instantiation name is left empty, which keeps the group name as the
  gtest suite name and lets `manifest.json` filter on `LibpngSize.*` and
  friends. Full names read
  `LibpngSize.Scenario/S00042_grayscale_1_bit_3x1wr_seq`.
- Scenario name suffixes are `[A-Za-z0-9_]` only, with a zero-padded index
  prefix. GoogleTest requires parameterized name suffixes to be unique and to
  hold only alphanumerics and underscores, and its filter expressions use `:`
  as a separator with `*`/`?` as wildcards, so upstream's descriptive names
  (`rgb 8 bit interlaced(libpng) 3x5`) cannot be used verbatim. The index
  prefix also keeps the gtest listing in upstream's execution order.
- **Harness zlib**: pngvalid calls zlib `crc32()` directly in the *harness*
  (independent of the library under test), so the test binary links the system
  zlib for its own use.
- The suite compares against **decoded/round-tripped pixels and computed
  expectations**, not re-encoded file bytes — file bytes depend on the zlib
  compressor and would differ for a translation using a different (valid) zlib.

## Excluded

- `pngtest.c` — its byte-for-byte re-encode comparison is compressor-sensitive
  (a translation using a different zlib produces different, still-valid bytes),
  so it is not a translation-fidelity test. `pngvalid` (pixel-level) is used
  instead.
- Other `contrib/libtests` programs (`pngstest`, `pngunknown`, `pngimage`) and
  the shell test scripts — could be added later; `pngvalid` is the core
  correctness validator.

## Reproduce

```sh
# from the repo root
harvest-bench verify tests/libpng
# -> libpng: 6521 tests, 6521 passed, 0 failed, 0 skipped
```

A bare run of the suite binary works too, and re-records each group once:

```sh
cmake -S test_case -B c_build -DCMAKE_BUILD_TYPE=Release
cmake --build c_build -j
cmake -S gtest_suite -B gtest_build -DCMAKE_BUILD_TYPE=Release \
      -DTEST_LIB_PATH=$(pwd)/c_build/libpng.so
cmake --build gtest_build -j
LD_LIBRARY_PATH=$(pwd)/c_build ./gtest_build/harvest_gtest
```
