# libpng Library Test Case

## Source

- **Project**: [libpng](https://github.com/pnggroup/libpng) — the official PNG reference library
- **Version**: libpng16 branch
- **License**: PNG Reference Library License (libpng license, permissive)
- **Dependency**: libpng calls **zlib** for DEFLATE (it does not implement compression itself)

## What This Tests

The complete libpng library for C-to-Rust translation. `test_case/` contains
**only** verbatim upstream library sources. There is no dispatch wrapper and no
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

## Tests: `gtest_suite/`

Validation wraps libpng's own correctness validator, **`pngvalid`**
(`contrib/libtests/pngvalid.c`, ~12k lines, verbatim). pngvalid self-generates
PNG images across bit depths, color types, and interlacing, writes and reads
them back through the public API, and checks the round-tripped pixels against
computed expectations with tight numeric tolerances. It needs no external
files.

`glue/glue_pngvalid.c` includes `pngvalid.c` verbatim (with `main` renamed and
`PNG_FREESTANDING_TESTS` defined so it resolves `<png.h>`/`<zlib.h>`/
`<cexcept.h>` from the include path rather than upstream-tree relative paths),
and exposes one entry point per pngvalid test group. `upstream_tests.cc` wraps
them as GoogleTest cases:

| Case | pngvalid group | Covers |
|---|---|---|
| `Libpng.PngvalidStandard`  | `--standard`  | baseline read/write, all bit depths / color types / interlacing |
| `Libpng.PngvalidSize`      | `--size`      | image size handling |
| `Libpng.PngvalidTransform` | `--transform` | read/write transform pipeline (expand, strip, gray↔rgb, ...) |
| `Libpng.PngvalidGamma`     | `--gamma`     | gamma correction (the numerically tightest group) |

### Notes

- **Process isolation**: pngvalid keeps global state across a run, so each group
  runs in its own process (`mode: "case"` in the manifest; the runner isolates
  case-mode tests).
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
# from the repo root, with cmake >= 3.24
harvest-bench verify tests/libpng
# -> libpng: 4 tests, 4 passed
```
