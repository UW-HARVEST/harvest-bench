# MuJS Library Test Case

## Source

- **Project**: MuJS — a lightweight, embeddable Javascript interpreter written in portable C99
- **Version**: v1.3.8 (per `JS_VERSION_MAJOR`/`JS_VERSION_MINOR`/`JS_VERSION_PATCH` in `mujs.h`)
- **License**: see upstream project — the original `COPYING` file was removed during
  formatting and was not transcribed here; verify the license against upstream before
  relying on this note.
- **Author**: Artifex Software, Inc.

## What This Tests

This test case provides the MuJS Javascript engine library (lexer, parser, bytecode
compiler, bytecode interpreter, and built-in object model) for C-to-Rust translation,
restricted to a pure "string in, result out" surface with no file I/O.

## Adaptation Layer

Changes made to the upstream source to fit the test corpus:

- **Removed `main.c`** — the interactive shell / script-runner executable. Not needed,
  since this test case targets the library, not an executable.
- **Removed `pp.c`** — the standalone pretty-printer executable. Same reason.
- **Removed from `jsstate.c`**: `js_loadfile`, `js_ploadfile`, and `js_dofile` — these
  were the only functions in the engine that called `fopen`. Removing them means the
  library now has no file I/O at all. `js_newstate` and `js_dostring` were kept as-is.
- Removed the matching declarations for those three functions from `mujs.h` so the
  public header stays consistent with the implementation.
- Removed build/codegen/doc files not needed to compile the library: `Makefile`,
  `one.c` (amalgamation helper), `genucd.py`, `SpecialCasing.txt`, `UnicodeData.txt`
  (Unicode data tables only used to regenerate the already-checked-in
  `astnames.h`/`opnames.h`/`utfdata.h`), `docs/`, `tools/` (test262 harness), `AUTHORS`,
  `COPYING`, `README`, `.gitattributes`.

`js_dostring` evaluates a script but discards its return value (it calls `js_pop`
immediately after `js_call`), so it can't be used as-is to get a result string back
out, and there is no `print()` or other output builtin in the library itself (that was
a host binding supplied by the now-removed `main.c`). The test suite therefore drives
the engine through this public-API call sequence, built entirely out of the
primitives that survived the file-I/O removal:

```
js_newstate → js_ploadstring (parse+compile, protected)
            → js_pcall (protected call; leaves the return value OR thrown error on the stack)
            → js_trystring (safely stringify whatever's on top)
            → js_freestate
```

Each test feeds a NUL-terminated Javascript source string through that sequence and
compares the stringified result (or thrown error) and the threw flag against the
expected values. The sequence lives in the test suite itself
(`gtest_suite/test262_vectors.cc`); everything in `src/`/`include/` is unmodified
mujs (minus the three removed functions).

## Source Files

| File | Lines | Role |
|---|---|---|
| `jslex.c` | 881 | Lexer |
| `jsparse.c` | 1,065 | Parser |
| `jscompile.c` | 1,428 | Bytecode compiler |
| `jsrun.c` | 2,097 | Bytecode interpreter |
| `jsstate.c` | 238 | Interpreter state, `js_newstate`, `js_dostring` |
| `jsvalue.c` | 708 | Value representation |
| `jsobject.c` | 564 | Object model |
| `jsproperty.c` | 341 | Property access |
| `jsarray.c` | 870 | `Array` prototype |
| `jsstring.c` | 866 | `String` prototype |
| `jsdate.c` | 861 | `Date` prototype |
| `jsnumber.c` | 198 | `Number` prototype |
| `jsboolean.c` | 38 | `Boolean` prototype |
| `jsmath.c` | 194 | `Math` object |
| `jsregexp.c` | 234 | `RegExp` prototype |
| `jsfunction.c` | 231 | `Function` prototype |
| `jsbuiltin.c` | 249 | Global built-in functions |
| `json.c` | 422 | `JSON.parse` / `JSON.stringify` |
| `jsdtoa.c` | 749 | Number ⇄ string conversion |
| `jsgc.c` | 284 | Garbage collector |
| `jserror.c` | 140 | Error objects |
| `jsrepr.c` | 285 | `repr()` / source printing |
| `jsintern.c` | 137 | String interning |
| `regexp.c` | 1,277 | Regex engine |
| `utf.c` | 305 | UTF-8 helpers |
| `include/mujs.h` | 250 | Public API |
| **Total** | **~14,950** | |

## Build

```bash
cmake -S test_case -B build-ninja -G Ninja
cmake --build build-ninja
```

## Tests: `gtest_suite/`

Validation is a GoogleTest suite run by the repository runner against the shared
library (C build or translated Rust cdylib) — see `test_case/CMakeLists.txt` (build)
and below (run).

- `test262_vectors.cc` — 260 single-call tests, a faithful translation of the retired
  `test_vectors/` JSON vectors (since removed) into one `TEST` per vector: each drives
  the public mujs API (`js_ploadstring` → `js_pcall` → `js_trystring`) with the
  vector's JS source (embedded verbatim as a raw string literal) and compares the
  stringified result (or thrown error) byte-for-byte against the expected output.

**mujs is ES5-only.** Confirmed directly from the source: `jslex.c`'s reserved-word
table has no `let`/`const`/`class`/`yield`/`async`/`await`/`static`/`super`/`of`; there
are no arrow-function/template-literal/spread/destructuring tokens in the
lexer/parser; and `jsB_init` (`jsbuiltin.c`) only wires up `Object`, `Array`,
`Function`, `Boolean`, `Number`, `String`, `RegExp`, `Date`, `Error`, `Math`, and
`JSON` — no `Symbol`/`Map`/`Set`/`Promise`/`Proxy`/`Reflect`/typed
arrays/`Atomics`/`Intl`. That directly shaped the generation approach below.

**Generation method** (tooling was not committed, matching this corpus's convention of
gitignoring `gen_vectors`/`gen_vectors.c`-style generators). This produced the original
`test_vectors/*.json` files, which were subsequently translated 1:1 into
`test262_vectors.cc` and removed:

1. Scanned `test262-main/test/language/{expressions,statements,types,literals,
   punctuators,keywords,reserved-words,future-reserved-words,identifiers,
   identifier-resolution,white-space,line-terminators,comments,asi,arguments-object,
   function-code,global-code,eval-code,statementList,source-text,directive-prologue}`
   and `test262-main/test/built-ins/{Array,Boolean,Date,Error,Function,JSON,Math,
   Number,Object,RegExp,String,NativeErrors,global,isNaN,isFinite,parseInt,parseFloat,
   decodeURI,decodeURIComponent,encodeURI,encodeURIComponent,eval,Infinity,NaN}` —
   deliberately excluding `test/intl402` (needs `Intl`) and `test/staging` (unstable
   proposals) entirely. (`test262-main/` was a temporary local checkout used only as
   raw material for this step; it was never part of `test_case/` or `gtest_suite/` and
   is not present in this directory.)
2. Kept only tests whose frontmatter has an empty/absent `features:` list (any feature
   tag implies post-ES5 functionality), and dropped `flags: [module, async, generated,
   raw, CanBlockIsFalse, CanBlockIsTrue]` (no module loader, no event loop for async
   completions, and `CanBlockIs*` targets Atomics/SharedArrayBuffer, all unsupported).
3. Flattened each survivor into one self-contained string: `harness/sta.js` +
   `harness/assert.js` (+ anything in the test's own `includes:`) + the test body,
   optionally prefixed with `"use strict";` for the strict-mode variant (default tests
   get both a sloppy and a strict variant; `onlyStrict`/`noStrict` get one each).
4. Ran every flattened candidate through the *actual compiled* `libmujs` (via the
   same load/call/stringify sequence the tests use) to get real, actual behavior — not
   merely trust the frontmatter's stated intent. 2,537 candidates were produced from 1,358 unique
   test262 source files.
5. Cross-checked actual behavior against test262's stated expectation (no `negative`
   block ⇒ expect no throw; `negative.type` ⇒ expect a throw whose stringified message
   contains that type name) and **kept only the 1,795 that matched**. The 742 that
   didn't are a mix of real mujs spec-compliance gaps (spot-checked example: mujs gives
   `Array.prototype.push.length` the wrong property descriptor, so
   `built-ins/Array/prototype/push/length.js` legitimately throws where the spec says
   it shouldn't) — these are genuine, not generation bugs, and are excluded on purpose
   since the goal is exercising correctly-implemented behavior, not enshrining known
   mujs bugs as "expected" output.
6. Randomly sampled 260 of the 1,795 matches, capped and rotated per source
   subdirectory for topic diversity, as the final curated set (a curated sample — not an attempt at exhaustive test262 coverage).
7. Verified all 260 for self-consistency against the compiled library, then translated
   each vector into a `TEST(Test262Vectors, ...)` case in `test262_vectors.cc`.

## Running Tests

```bash
# Standalone against the C build (from this directory):
cmake -S test_case -B c_build -DCMAKE_BUILD_TYPE=Release
cmake --build c_build -j
cmake -S gtest_suite -B gtest_build -DCMAKE_BUILD_TYPE=Release \
      -DTEST_LIB_PATH=$(pwd)/c_build/libmujs.so
cmake --build gtest_build -j
LD_LIBRARY_PATH=$(pwd)/c_build ./gtest_build/harvest_gtest

# Through the repository runner (manifest-aware):
harvest-bench run --suite gtest_suite --lib <path-to-your-libmujs.so>
```
