# Upstream zstd test programs (ported)

Verbatim copies from the zstd repository, tag **v1.5.7**, wrapped for
GoogleTest via the `glue/glue_*.c` translation units. The upstream files themselves
are **unmodified**; each glue TU includes one of them with `main` renamed and
exposes deterministic `harvest_*` entry points that `upstream_tests.cc`
wraps as test cases. `../golden_tests.cc` drives the golden data sets through
the library API directly (upstream only consumes them via CLI tests).

| File | Origin | Role |
|---|---|---|
| `fuzzer.c` | `tests/fuzzer.c` | Single-shot / block / dictionary / advanced-parameter tests (unit sections + fixed-seed fuzz loop) |
| `zstreamtest.c` | `tests/zstreamtest.c` | Streaming API tests, old and new API surfaces (unit section + fixed-seed fuzz loops) |
| `longmatch.c` | `tests/longmatch.c` | Long-distance match handling (streaming) |
| `bigdict.c`, `largeDictionary.c` | `tests/` | >=1 GiB dictionary windows (heavy: memory + wall clock) |
| `invalidDictionaries.c` | `tests/` | Malformed dictionaries must be rejected |
| `seqgen.c/.h`, `external_matchfinder.c/.h` | `tests/` | zstreamtest dependencies (sequence generator, external sequence producer) |
| `datagen.c/.h`, `util.c/.h`, `timefn.c/.h`, `platform.h` | `programs/` | Support code, compiled in per upstream's own `*-dll` recipes |
| `lib/**/*.h`, `lib/common/xxhash.c` | copies of `../../test_case/src` | Library headers (incl. internal ones the tests include) and the oracle-side xxhash implementation |
| `golden-*/`, `dict-files/` | `tests/` | Golden data sets, driven by `../golden_tests.cc` |

## Determinism

The unit-test sections run with constant seed 0 — exactly what upstream's own
`main()` does ("constant seed for predictability"). Fuzz loops take a pinned
seed with the wall-clock cutoff disabled and fixed cycle counts. The golden
tests are pure data-driven. No test depends on time or randomness.

## Single-threaded library shim

The library under test is built **without ZSTD_MULTITHREAD** (that is the
adaptation's translation contract). Upstream always builds these tests
against an MT library and unconditionally sets the MT-only parameters
(`ZSTD_c_nbWorkers`, `ZSTD_c_jobSize`, `ZSTD_c_overlapLog`,
`ZSTD_c_rsyncable`). `glue_mt_shim.c` interposes the two parameter-setting
entry points at link level and clamps those four parameters to 0
(synchronous mode). Compressed-output semantics are unchanged; MT job
scheduling itself goes unexercised. The `#ifdef ZSTD_MULTITHREAD` test
sections (threadPoolTests etc.) compile out.

## Scenario-level reporting for the unit-test monoliths

Upstream packs dozens of independent scenarios into a single unit-test
function that aborts at the first failure. Wrapping such a function as one
GoogleTest case would collapse all of them into a single pass/fail bit, so
each scenario is exposed as its own test case instead — the harness enumerates
them with `--gtest_list_tests`, which keeps one gtest case equal to one scoring
point and needs no protocol change.

The upstream function still executes only once per library under test:

- The glue TU intercepts the progress banners upstream already prints (by
  defining `fprintf` as a macro before including the upstream file, so its own
  `DISPLAY` macro routes through the hook) and records each scenario boundary.
  The upstream sources are not modified.
- Language split: only the translation units that `#include` an upstream test
  source are C, because those sources are not valid C++ (implicit `void*`
  conversions, and whatever C-only constructs upstream may add later). They are
  kept as thin as possible; the recording machinery (`scenario_record.cc`) and
  the scenario tables/banner parsing live in C++ files reached through a plain
  C interface. Nothing may throw across that boundary — an exception unwinding
  through upstream's C frames would be undefined behaviour — so every entry
  point is `noexcept`.
- The run happens in a **forked child** that appends to a record file and
  flushes after every scenario. A crashing or hanging library therefore still
  leaves a usable partial record, and because the parent survives, scenarios
  that already succeeded are still reported as passing.
- The child's deadline is the harness-provided soft timeout for the current
  test (`budgets.json` exported unchanged as `HARVEST_TEST_SOFT_TIMEOUT_SECS`;
  the harness SIGKILLs a grace period later), so the partial record reaches
  disk before the kill. `budgets.json` is the single authority on time.
- The record is keyed to the library under test (path, mtime, size) and reused
  by subsequent test processes, so the harness's one-process-per-test model does
  not re-run the function once per scenario.

Verdicts: a scenario followed by another start (or by the run completing)
passed; the last one started before an abort failed; scenarios never started
are reported as **failures, not skips** — skipped tests leave the harness's
denominator, which would let an early crash score better than a late one.

## Deliberately not ported

- `legacy.c` — its embedded test input is a single concatenated stream that
  starts with a v0.4.3 frame; the adapted library build is
  `ZSTD_LEGACY_SUPPORT=5` (v0.5+ only), which rejects it with "Unknown frame
  descriptor", and the stream cannot be subset without modifying the upstream
  file. Portable if the library contract is ever changed to
  `ZSTD_LEGACY_SUPPORT=4`.
- `decodecorpus.c` — statically embeds the library sources
  (`#include "compress/zstd_compress.c"`), so it would test its own embedded
  copy of upstream C rather than the shared library under test.
- `cli-tests/`, `playTests.sh` — exercise the `zstd` CLI binary, not the library.
- `fuzz/` + `regression/` — libFuzzer harnesses; different execution model.
- `fullbench.c`, `paramgrill.c`, `*speed*.py` — performance benchmarks (perf
  tests excluded by policy).
- `poolTests.c` — thread-pool only; the library under test is single-threaded.
- `checkTag.c`, `check_size.py`, `libzstd_builds.sh`, `versions` scripts — release/build infrastructure.
- `roundTripCrash.c` — libFuzzer-corpus companion tool (file-at-a-time crash reproducer).

## License

zstd (including `tests/` and `programs/`) is dual-licensed **BSD-3-Clause OR
GPL-2.0** at the user's option (see `LICENSE` in this directory); these copies
are used under the BSD license.
