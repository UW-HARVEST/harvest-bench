# PCRE2 Library Test Case

## Source

- **Project**: [PCRE2](https://github.com/PCRE2Project/pcre2) — Perl-Compatible Regular
  Expressions, version 2 API
- **Version**: 10.48-DEV (per `PCRE2_MAJOR`/`PCRE2_MINOR`/`PCRE2_PRERELEASE` in `pcre2.h`)
- **License**: BSD-3-Clause-with-exception (library and test driver). The `testdata/`
  directory is public domain — see `gtest_suite/upstream/README.md` for details.
- **Author**: Philip Hazel (original author). Nicholas Wilson and Zoltán Herczeg
  maintain it since 2024.

## What This Tests

This test case provides the PCRE2 regular-expression engine (pattern compiler, matcher,
DFA matcher, and substitution engine) for C-to-Rust translation. The build uses the
8-bit code unit width with Unicode/UTF-8 support (`SUPPORT_UNICODE`) enabled.
`test_case/` contains **only** verbatim upstream library sources and upstream API
documentation (`doc/`). There is no dispatch
wrapper and no other adaptation code in the translation input. The translated Rust
cdylib must therefore export the **full public C ABI** of the library. The test
suite links against it directly (`pcre2_compile_8`, `pcre2_match_8`,
`pcre2_dfa_match_8`, `pcre2_substitute_8`, ...).

## Adaptation Layer

Changes made to the upstream source to fit the test corpus:

- **Removed the two executables** — `pcre2test.c` (the interactive/scripted test-driver
  tool) and `pcre2grep.c` (the grep-alike CLI). Neither is needed, because this test
  case targets the library, not an executable. Both have their own `main()` and are
  separate CMake targets upstream. (`pcre2test.c` reappears, unmodified, under
  `gtest_suite/upstream/` — see below.)
- **Removed other tool/demo/self-test sources not part of the library itself**:
  `pcre2_dftables.c` (the chartable-generator tool — see below for how its output was
  used instead), `pcre2demo.c`, `pcre2_jit_test.c`, `pcre2posix_test.c`,
  `pcre2_fuzzsupport.c` (libFuzzer entry point), and `pcre2posix.c`/`pcre2posix.h` (the
  separate POSIX `regcomp`/`regexec` wrapper library, its own CMake target upstream, not
  part of the core 8-bit library).
- **`pcre2_chartables.c`**: upstream generates this file at build time, either with
  the built `pcre2_dftables` tool or — the common/default path — as a copy of the
  checked-in `pcre2_chartables.c.dist`. We took the simpler of upstream's own two
  options: `pcre2_chartables.c` here is `pcre2_chartables.c.dist` copied over unmodified.
- **`include/pcre2.h`**: upstream generates the real public header from `pcre2.h.in` via
  CMake `@VAR@` substitution (mainly to bake in version numbers). Instead we used
  `pcre2.h.generic` as-is. Its own header comment states that it exists specifically
  for builds done "by hand" without configure/CMake.
- **`test_case/src/config.h`**: upstream generates `config.h` from `src/config-cmake.h.in`
  via CMake substitution. We used `config.h.generic` as-is instead, built with
  `-DHAVE_CONFIG_H`. Its defaults — `LINK_SIZE`, `MATCH_LIMIT`, `MAX_NAME_SIZE`,
  `NEWLINE_DEFAULT`, `PARENS_NEST_LIMIT`, `HEAP_LIMIT`, `PCRE2_EXPORT`, etc. — are all
  self-contained `#ifndef`-guarded defaults designed for exactly this scenario.

## Source Files

| File | Lines | Role |
|---|---|---|
| `pcre2_compile.c` + `pcre2_compile.h` | 11,706 | Pattern compiler (main entry point) |
| `pcre2_compile_cgroup.c` | 632 | Compiler: capture-group bookkeeping |
| `pcre2_compile_class.c` | 2,770 | Compiler: character-class parsing |
| `pcre2_auto_possess.c` | 1,416 | Compiler: auto-possessification optimization |
| `pcre2_convert.c` | 1,261 | Compiler: glob/POSIX pattern conversion |
| `pcre2_study.c` | 2,086 | Compiler: pattern study / minimum-length analysis |
| `pcre2_match.c` | 8,254 | Backtracking matcher (main runtime entry point) |
| `pcre2_dfa_match.c` | 4,134 | Alternative DFA-based matcher |
| `pcre2_match_data.c` + `pcre2_match_next.c` | 355 | Match-data (ovector) management |
| `pcre2_substitute.c` | 1,795 | `pcre2_substitute()` |
| `pcre2_substring.c` | 553 | Captured-substring extraction |
| `pcre2_jit_compile.c` | 14,431 | JIT compiler (compiled in; inert — `SUPPORT_JIT` is off) |
| `pcre2_ucd.c` + `pcre2_ucptables_inc.h` | 7,444 | Unicode character-property tables (static data) |
| `pcre2_xclass.c` | 547 | Extended (Unicode-aware) character-class matching |
| `pcre2_valid_utf.c` | 397 | UTF-8 validation |
| `pcre2_extuni.c` | 159 | Extended Unicode grapheme cluster matching |
| `pcre2_script_run.c` | 343 | Unicode script-run assertions |
| `pcre2_ord2utf.c` | 118 | Codepoint → UTF-8 encoding |
| `pcre2_pattern_info.c` | 430 | `pcre2_pattern_info()` |
| `pcre2_context.c` | 557 | General/compile/match/convert context objects |
| `pcre2_config.c` | 250 | `pcre2_config()` |
| `pcre2_error.c` | 386 | `pcre2_get_error_message()` |
| `pcre2_newline.c` | 239 | Newline-convention handling |
| `pcre2_find_bracket.c` | 217 | Bracket/group lookup helper |
| `pcre2_maketables.c` + `pcre2_chartables.c` | 369 | Character tables (the latter from `.c.dist`) |
| `pcre2_tables.c` | 310 | Static string/name tables |
| `pcre2_serialize.c` | 295 | Compiled-pattern (de)serialization |
| `pcre2_string_utils.c` | 199 | String utility helpers |
| `pcre2_chkdint.c` | 94 | Checked integer-overflow arithmetic |
| `pcre2_internal.h` + `pcre2_intmodedep.h` + `pcre2_ucp.h` + `pcre2_util.h` | 4,066 | Private headers |
| `pcre2_jit_*_inc.h` + `pcre2_printint_inc.h` | 6,320 | JIT/debug source fragments (`#include`d, not compiled standalone) |
| `config.h` | 497 | From `config.h.generic` (see Adaptation Layer) |
| `include/pcre2.h` | 1,080 | Public API, from `pcre2.h.generic` |
| **Total** | **~73,700** | |

## API Documentation: `test_case/doc/`

Upstream API documentation for the agents that translate and verify this
library. `pcre2.h` lists the option bits but not the rules for them, for
example which options `PCRE2_LITERAL` allows or which options `pcre2_match`
accepts.

- **Source**: PCRE2 commit `ff92e0b` (`git describe` pcre2-10.47-57-gff92e0b9,
  10.48-DEV). This is the commit whose library files are identical to
  `test_case/`.
- **Files**: these man pages, rendered to plain text: `pcre2api`,
  `pcre2syntax`, `pcre2limits`, `pcre2matching`, `pcre2partial`, `pcre2jit`,
  `pcre2convert`, `pcre2serialize`, `pcre2unicode`; and `LICENCE.md`. Render
  command: `groff -t -man -Tascii -rLL=100n -rHY=0 -dAD=l -P-cbou doc/<page>.3`
  (no hyphenation, so option names are not split). This build has no JIT
  support, so the JIT functions return errors; `pcre2jit.txt` describes that
  case too.
- **Not included**: `pcre2pattern` (regex semantics, long, no parameter
  rules), the per-function pages, the HTML copy, and `pcre2test`/`testdata/`
  (the test oracle).

## Tests: `gtest_suite/`

Validation is a GoogleTest suite that the repository runner executes against the
shared library (C build or translated Rust cdylib). See
`gtest_suite/upstream/README.md` for provenance details.

- `upstream_tests.cc` + `upstream/` — the upstream `pcre2test.c` test-driver program
  (unmodified). The wrapper turns each of `RunTest`'s numbered tests into one
  `pcre2test` invocation and diffs its output byte-for-byte against the real
  upstream `testoutput*` file. 14 tests are ported this way. They cover the main
  non-UTF/UTF matching functionality, DFA matching, `\C` handling, serialization,
  pattern conversion, and Unicode property tests. `pcre2test.c` runs once per
  process, the way the real binary does under `RunTest`, so each invocation gets its
  own forked child process. Locale-specific, 16/32-bit-specific, POSIX-interface,
  JIT-available, and EBCDIC tests are out of scope for this build's configuration.
  Three more (5, 7, 10) hit an upstream disassembler bug unrelated to the library
  under test. See `gtest_suite/upstream/README.md` for the full rationale on both
  the selected and excluded tests.

There is no hand-written vector suite here. The upstream `pcre2test` data files
already give comprehensive, byte-exact-verifiable coverage on their own.

## Build

```bash
cmake -S test_case -B build-ninja -G Ninja
cmake --build build-ninja
```

## Running Tests

```bash
# Standalone against the C build (from this directory):
cmake -S test_case -B c_build -DCMAKE_BUILD_TYPE=Release
cmake --build c_build -j
cmake -S gtest_suite -B gtest_build -DCMAKE_BUILD_TYPE=Release \
      -DTEST_LIB_PATH=$(pwd)/c_build/libpcre2.so
cmake --build gtest_build -j
LD_LIBRARY_PATH=$(pwd)/c_build:$(pwd)/gtest_build ./gtest_build/harvest_gtest

# Through the repository runner (manifest-aware):
harvest-bench run --suite gtest_suite --lib <path-to-your-libpcre2.so>
```

The suite also builds the `pcre2_posix_glue` shared library into `gtest_build/`
(see `gtest_suite/upstream/README.md` for why it exists). That is why `gtest_build`
must be on `LD_LIBRARY_PATH` alongside `c_build`. The library name and extension
vary by platform: macOS builds `libpcre2.dylib` and needs `DYLD_LIBRARY_PATH`
instead.

**Verification status**: both `test_case` and `gtest_suite` build cleanly, and all 14
ported tests pass, byte-for-byte, against the C build's `libpcre2.dylib`/`.so`.
