# libsodium Library Test Case

## Source

- **Project**: [libsodium](https://github.com/jedisct1/libsodium) — modern, easy-to-use software library for encryption, decryption, signatures, password hashing, and more
- **Version**: 1.0.23 (library version 30.0)
- **License**: ISC (library and tests)
- **Author**: Frank Denis

## What This Tests

The complete libsodium library for C-to-Rust translation. `test_case/` contains
**only** verbatim upstream library sources (`src/libsodium/`). There is no
dispatch wrapper and no other adaptation code in the translation input. The
translated Rust cdylib must therefore export the **full public C ABI** of the
library. The test suite links against the public symbols directly
(`crypto_box`, `crypto_secretbox`, `crypto_sign` (Ed25519), `crypto_aead_*`,
`crypto_generichash` (BLAKE2b), SHA-2/SHA-3, `crypto_pwhash` (Argon2/scrypt),
`crypto_scalarmult` (X25519), `crypto_kx`, `crypto_kem` (ML-KEM-768, X-Wing),
`randombytes`, `sodium_*` utilities, ...).

## Source Files

All files are taken **verbatim** from libsodium 1.0.23 (`src/libsodium/`), with
one generated header:

- **145 `.c` + 183 `.h`** across the `crypto_*`, `randombytes`, and `sodium`
  subtrees.
- `include/sodium/version.h` is generated from the upstream `version.h.in`
  template (autotools normally does this at configure time): `@VERSION@`→1.0.23,
  major→30, minor→0.

No `HAVE_*` feature macros are defined, so every `#ifdef HAVE_*` selects the
portable fallback — equivalent to `./configure --disable-asm`. This is the
reference semantics; SIMD/hardware variants are not exercised.

The only non-upstream file is `test_case/CMakeLists.txt`, which builds the
sources into a single shared library (`libsodium.so`).

## Tests: `gtest_suite/`

Validation is a GoogleTest suite that the repository runner executes against
the shared library (C build or translated Rust cdylib).

The suite wraps the **104 upstream test drivers** from libsodium's own
`test/default/` suite. Each driver is a self-contained known-answer test: it
calls the public API and prints results, and its harness (`cmptest.h`) byte-
compares that output against the driver's frozen `.exp` golden file, returning
0 on an exact match. The upstream drivers and `.exp` files are **unmodified**.

Each `glue/glue_<name>.c` includes one upstream driver verbatim, renaming its
`main`/`xmain` uniquely (the 104 drivers are linked into one binary, so their
file-scope symbols would otherwise collide). `upstream_tests.cc` wraps each as
one GoogleTest case (`Libsodium.<name>`).

| Directory | Origin | Role |
|---|---|---|
| `upstream/*.c`, `*.exp` | libsodium `test/default/` (verbatim) | 104 known-answer drivers + golden outputs |
| `upstream/cmptest.h`, `quirks.h` | libsodium `test/` (verbatim) | driver harness (stdout capture + `.exp` diff) |
| `glue/glue_*.c` | this repo | include-and-rename shims → `harvest_<name>()` |
| `upstream_tests.cc` | this repo | 104 GoogleTest cases |
| `manifest.json` | this repo | per-case process budgets |

### Process isolation (important)

Each driver relies on `cmptest.h`'s file-scope state (a shared `fp_res` handle,
a `sodium_malloc` guard page, hardcoded test buffers) and writes a `<name>.res`
scratch file. Drivers therefore must each run in **their own process**
(`mode: "case"` in the manifest) — running all 104 in one process makes them
interfere. The repository runner already runs case-mode tests one-per-process;
do **not** group these into `suite`-mode entries.

## Excluded

- `test/default/` non-driver files (`run.sh`, `wasi-test-wrapper.sh`, etc.) —
  build/CI plumbing, not correctness tests.
- `test/vectors/` (Zig-based tooling) and `test/symbols/` (a symbol-list text
  file) — neither is a library correctness test.
- SIMD/hardware code paths — the portable fallback is the reference.

## Reproduce

```sh
# from the repo root, with cmake >= 3.24
harvest-bench verify tests/libsodium
# -> libsodium: 104 tests, 104 passed
```
