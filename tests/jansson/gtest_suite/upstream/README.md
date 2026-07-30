# Upstream jansson test material (ported)

Verbatim copies from the jansson repository, tag **v2.15.0**, wrapped for
GoogleTest via the glue TUs in `../glue/`. The upstream files themselves are
**unmodified**. Each glue TU includes one of them (with `main` renamed) and
exposes a `harvest_*` entry point that `../upstream_tests.cc` (API programs) or
`../data_tests.cc` (data suites, via `json_process`) wraps as test cases.

| Path | Origin | Role |
|---|---|---|
| `api/test_*.c` | `test/suites/api/` | 20 procedural API unit-test programs |
| `api/util.h` | `test/suites/api/` | Shared harness header (defines `main`, declares `run_tests`) |
| `json_process.c` | `test/bin/` | Data-suite driver: parse `input` (+`env`), dump, compare to `output`/`error` |
| `valid/`, `invalid/`, `invalid-unicode/`, `encoding-flags/` | `test/suites/` | 119 data-driven case directories |
| `include/*.h` | copies of `../../test_case/include/` | Public API header + config headers |

## How the two mechanisms map to GoogleTest

- **API programs** (`api/test_*.c`): each defines `run_tests()` and gets its
  `main()` from `util.h`. The glue renames `main` and calls it. A nonzero
  return (upstream `fail()`/`exit(1)`) is a test failure. `test_chaos_realloc.c`
  shares several file-scope symbol names with `test_chaos.c`. Its glue
  `#define`-renames them to avoid a cross-TU link collision (upstream files
  untouched).
- **Data suites**: the driver reuses `json_process.c` verbatim. It parses each
  case's `input` (honoring an optional `env` config: indent/compact/
  ensure_ascii/preserve_order/sort_keys/precision/strip/HASHSEED) and compares
  the output against the case's expected `output` (valid) or `error` (invalid)
  file — the same logic upstream's `run` script invokes. The driver zeroes the
  file-scope `conf` before each call, so cases stay independent within one
  process.

## Determinism / semantics

- Object member order is insertion order in jansson, so serialized output is
  deterministic regardless of hash seed.
- The suite exercises both the `normal` and whitespace-`strip` variants that
  upstream runs. Cases whose `input` contains an embedded NUL carry a `nostrip`
  marker (the strip path is C-string/`strlen`-based and would truncate at the
  NUL). The data test honors that marker exactly as upstream's `run` does.
- Cases with `skip_unless_dtoa` / `skip_if_dtoa` are gated on the library
  build's dtoa support via `GTEST_SKIP()`. This corpus builds with
  `DTOA_ENABLED`.
- The API tests install process-global allocator hooks (`json_set_alloc_funcs`
  in the memory-func and chaos tests). They stay isolated because the runner
  starts one process per test. A run of the whole binary in a single process
  can bleed allocator state between tests and is not the validation path.

## Deliberately not ported

- `test/bin/json_process.c` is reused as a helper, not wrapped as a test.
- Shell/Python harness scaffolding (`run`, `run-tests.sh`, `scripts/`,
  `check-exports`) — the GoogleTest driver replaces it.
- `test/ossfuzz/` — libFuzzer harness with a different execution model.
- No performance/benchmark material exists in jansson's test tree to exclude.

## License

jansson is **MIT**-licensed (see `LICENSE` in this directory). These copies are
used under that license.
