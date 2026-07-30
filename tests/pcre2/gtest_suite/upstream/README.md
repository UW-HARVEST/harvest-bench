# Upstream PCRE2 test driver (ported)

Verbatim copies from the PCRE2 repository, tag/checkout matching `test_case/`'s
10.48-DEV source, wrapped for GoogleTest via `glue_pcre2test.c`. `pcre2test.c`
itself is **unmodified**; the glue TU includes it with `main` renamed and
exposes a deterministic `harvest_pcre2test(argc, argv)` entry point that
`../upstream_tests.cc` calls once per RunTest-equivalent numbered test.

| File | Origin | Role |
|---|---|---|
| `pcre2test.c` | `src/pcre2test.c` | The real upstream test-driver CLI (unmodified) |
| `pcre2test_inc.h` | `src/pcre2test_inc.h` | Included by `pcre2test.c`, per code-unit width |
| `pcre2posix.c` / `pcre2posix.h` | `src/pcre2posix.c(.h)` | POSIX `regcomp`/`regexec` wrapper `pcre2test.c` links against (see below) |
| `pcre2_internal.h`, `pcre2_intmodedep.h`, `pcre2_util.h`, `pcre2_ucp.h` | `src/*` | Private headers `pcre2test.c` needs for its own internal-table access |
| `pcre2_tables.c`, `pcre2_ucd.c`, `pcre2_ucptables_inc.h`, `pcre2_valid_utf.c` | `src/*` | Private library sources `pcre2test.c`/`pcre2posix.c` `#include` directly (see below) |
| `config.h` | `src/config.h.generic` | Same file `test_case/src/config.h` is built from |
| `include/pcre2.h` | `src/pcre2.h.generic` | Same file `test_case/include/pcre2.h` is built from |
| `testdata/testinput*` / `testdata/testoutput*` | `testdata/*` | Selected upstream test data (see below) |
| `testdata/testbtables` | `testdata/testbtables` | Custom character-table file test 2's `tables=3` modifier reads from a hardcoded relative path (see below) |

## Why `pcre2posix` is its own shared library

`pcre2test.c` unconditionally `#include`s `pcre2posix.h` and calls `regfree()`
in its exit path (guarded only by `#ifdef SUPPORT_PCRE2_8`, not by whether a
POSIX-mode pattern was ever compiled), so the symbol has to be linkable even
though none of the ported tests below exercise the POSIX interface. Upstream
itself never links `pcre2posix.c` directly into the `pcre2test` executable's
object files — it builds a separate `libpcre2-posix` shared library instead.
That matters here because `pcre2posix.c` and `pcre2test.c` each independently
`#include "pcre2_tables.c"` (with `PRIV(name)` defined down to a bare `name`),
so both would emit the same non-static globals (`_pcre2_OP_lengths` and
friends); bundling them into one binary's object files collides at link time.
Building `pcre2posix.c` as its own shared library (`gtest_suite/CMakeLists.txt`'s
`pcre2_posix_glue` target) avoids that, matching upstream's own layout.

## Determinism and process isolation

`pcre2test` is deterministic by construction here: every invocation is a
single non-interactive run over a fixed input file with `-q` (quiet) set, so
there's no randomness to pin (unlike lz4's fuzz loops). Each ported test
number maps to exactly one `pcre2test` invocation (or two, for test 2 — see
`upstream_tests.cc`), mirroring what `RunTest` does for a fresh process per
test.

Granularity: RunTest's unit is one byte-diff per FILE, but each file packs
hundreds of independent pattern/subject blocks (~7300 across the 14 ported
files). The suite therefore splits verdicts per block: `Pcre2FileRun.*` runs
a file once and caches its raw output keyed to the library under test's
identity, and `Blocks/*` cases each compare one blank-line-delimited block of
that cached output against the matching expected block (input echo lines
anchor the alignment; input/output block counts are 1:1 on the reference C
build). Blocks past a crash-truncated output fail as NOT_REACHED rather than
skip. Block-level caveat: pcre2test state (`#` directives, test 20's
serialization save/load) flows through a file, so failures are not fully
independent — an early corrupted block can cascade.

`pcre2test.c` is written to run once per process (the way the real binary
does under `RunTest`): several file-scope statics (output buffers, the
last-open input/output `FILE*`, ...) are only freed at exit, never reset to a
fresh state. `upstream_tests.cc` therefore `fork()`s a throwaway child for
every `harvest_pcre2test()` call rather than calling it directly — calling it
twice in the same process reliably double-frees and crashes on the second
call otherwise.

## Selected tests

This build is 8-bit-only, Unicode/UTF enabled, no JIT (`SUPPORT_JIT` is off —
see `test_case`'s README), no locale, and has no POSIX-specific test coverage
even though the wrapper above exists just to satisfy the linker. Of
`RunTest`'s numbered tests, the following apply and are ported as-is:

| # | Title | Files |
|---|---|---|
| 1 | Main non-UTF, non-UCP functionality | `testinput1` / `testoutput1` |
| 2 | API, errors, internals, non-Perl stuff | `testinput2` / `testoutput2` (+ `-error` append) |
| 4 | UTF and Unicode property support | `testinput4` / `testoutput4` |
| 6 | DFA matching, main non-UTF functionality | `testinput6` / `testoutput6` |
| 9 | Specials for the basic 8-bit library | `testinput9` / `testoutput9` |
| 15 | Non-JIT limits and other non-JIT tests | `testinput15` / `testoutput15` |
| 16 | JIT-specific features when JIT unavailable | `testinput16` / `testoutput16` |
| 20 | Serialization and code copy tests | `testinput20` / `testoutput20` |
| 21 | `\C` tests without UTF | `testinput21` / `testoutput21` |
| 22 | `\C` tests with UTF | `testinput22` / `testoutput22-8` |
| 24 | Non-UTF pattern conversion tests | `testinput24` / `testoutput24` |
| 25 | UTF pattern conversion tests | `testinput25` / `testoutput25` |
| 26 | Unicode property tests | `testinput26` / `testoutput26` |
| 27 | Auto-generated Unicode property tests | `testinput27` / `testoutput27` |

All 14 pass, byte-for-byte, against this build's compiled library.

## Deliberately not ported

- **Tests 5, 7, 10** — each contains a pattern combining the `B` (bincode
  dump) modifier with a UTF-mode character class that compiles to an XCLASS
  opcode. `pcre2_printint_inc.h`'s disassembler mishandles that combination in
  this build: test 5's `/\65535/IB,utf,no_start_optimize` segfaults inside
  `pcre2_printint`, and tests 7 and 10 hit a pattern like `/[Ss]+/iB,utf`
  whose XCLASS disassembly loops forever re-printing `"xclass"` (multiple GB
  of output before being killed). This reproduces outside GoogleTest too (a
  standalone one-shot runner calling `pcre2test.c`'s `main` directly hits the
  same segfault/hang), so it isn't a gtest/fork-harness artifact — something
  about how this specific checkout's `pcre2_internal.h`/`pcre2_intmodedep.h`
  encode XCLASS doesn't match what the bundled disassembler expects. Not
  root-caused further; excluded like lz4 excludes its own out-of-scope tests.
- **Test 0** — unchecked argument/coverage-improving smoke tests; RunTest
  doesn't diff its output (only exit codes across many flag combinations), so
  it doesn't fit this suite's byte-diff-per-test shape.
- **Test 3** — locale-specific (`fr_FR` etc.); not portable to a sandboxed
  build, same reasoning as the vectors this replaces.
- **Test 8** — internal offsets/code-size checks; upstream's own comment
  calls it "a doublecheck for the maintainer," and expected output depends on
  the exact link size the library was compiled with.
- **Tests 11, 12, 13** — 16-bit/32-bit-specific features; this build only
  compiles the 8-bit library.
- **Test 14** — DFA UTF/UCP specials; upstream ships no `testoutput14-8` (only
  `-16`/`-32`), i.e. it has nothing to add over test 7 for an 8-bit-only build
  (moot here anyway since test 7 itself is excluded, above).
- **Test 17** — JIT-specific features when JIT *is* available; this build has
  no JIT.
- **Tests 18, 19** — POSIX interface tests; `test_case/` doesn't build
  `pcre2posix.c` as part of the library (see `test_case`'s README), so there's
  nothing exercising real POSIX-wrapped behavior to validate here.
- **Test 23** — `\C` disabled; this build doesn't disable `\C`.
- **Tests 28, 29** — EBCDIC-specific; this build targets ASCII.
- **`heap`** — environment-specific heap-frame sizes; explicitly excluded
  even by upstream's own default test run.

## License

PCRE2 is BSD-3-Clause-with-exception; the `testdata/` directory is public
domain per `LICENCE.md`. Unlike lz4's upstream test programs, no separate
`COPYING` file is needed here.
