# Upstream lz4 test programs (ported)

Verbatim copies from the lz4 repository, tag **v1.10.0** (commit `ebb370c`),
wrapped for GoogleTest via the `glue_*.c` translation units. The upstream
files themselves are **unmodified**; each glue TU includes one of them with
`main` renamed and exposes deterministic `harvest_*` entry points that
`../upstream_tests.cc` wraps as test cases.

| File | Origin | Role |
|---|---|---|
| `fuzzer.c` | `tests/fuzzer.c` | Block/HC/stream API tests (unit section + fixed-seed fuzz loop) |
| `frametest.c` | `tests/frametest.c` | LZ4F frame API tests (unit section + fixed-seed fuzz loop) |
| `roundTripTest.c` | `tests/roundTripTest.c` | Compress/decompress round trip (pinned level + pinned buffer) |
| `decompress-partial.c` | `tests/decompress-partial.c` | Partial-decode checks |
| `decompress-partial-usingDict.c` | `tests/decompress-partial-usingDict.c` | Partial-decode with dictionary |
| `util.h`, `platform.h` | `programs/` | Self-contained helper headers the tests include |
| `include/*.h` | copies of `../../test_case/include/` | Public lz4 headers (the suite must build without `test_case/`) |

## Determinism

All randomness is pinned: fuzz loops take an explicit seed (42) with the
wall-clock cutoff disabled, and the round-trip test uses a fixed LCG buffer
and explicit compression levels (upstream's default picks the level from a
content hash). The `fuzzer.c` unit-test section is upstream-gated behind
"no seed given" (which would randomize the subsequent loop); the glue calls
it directly instead, keeping both parts deterministic.

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

- `tests/test-lz4-*.sh`, `test-lz4-*.py` — exercise the `lz4` CLI binary,
  not the library; out of scope for a shared-library suite.
- `fullbench.c` — performance benchmark (perf tests excluded by policy).
- `abiTest.c` — cross-version ABI check; needs multiple installed versions.
- `freestanding.c` — freestanding-environment build test.
- `lz4_all.c` — amalgamated-build compile test.
- `datagen*`, `checkFrame.c`, `checkTag.c`, `loremOut.c` — support tools
  for the shell tests, not tests themselves.

## License

Upstream `tests/` and `programs/` files are **GPL-2.0-or-later** (see
`COPYING` in this directory), unlike the lz4 library itself (BSD 2-Clause).
The copied `include/` headers are from the library and remain BSD 2-Clause.
