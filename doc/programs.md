# Programs with a library, an executable, or both

A test program can have a shared library, an executable, or both. For
example, SQLite produces `libsqlite3.so` and the `sqlite3` shell, which links
the same engine. The suite's `manifest.json` declares this in a `program`
block. A manifest without a `program` block declares a library-only program,
which is the contract of the original suites.

## The `program` block

```json
{
  "program": {
    "lib": { "c_target": "toy" },
    "bin": { "c_target": "toyexe", "name": "toy" }
  },
  "test_types": [
    { "type": "bin", "filter": "ToyBin.*" }
  ],
  "invocations": [ ... ]
}
```

- `lib` and `bin` are the two dimensions of a program. Each sub-block that is
  present declares one. A `program` block must declare at least one.
- `c_target` is the CMake target in `test_case/CMakeLists.txt` that builds
  the artifact: a shared library target for `lib`, an executable target for
  `bin`. If you omit it, the build must have exactly one target of that type.
  The runner finds the file through the CMake file API, so the output name
  and the output directory of the target do not matter.
- `bin.name` is the name of the program. The runner passes it to the suite
  as `TEST_BIN_NAME`, and tests give it to the executable as `argv[0]`, so
  output that contains the program name does not change with the file name
  of the artifact. The manifest is its only source: a suite must not
  hard-code it. A translation names its executable `bin.name`.
- Unknown fields are errors at every level of the manifest, so a misspelled
  key cannot change the run silently. The top level and each invocation
  accept a free-form `note`.

The default CMake configuration of `test_case/` must build every artifact
that the program declares. The manifest cannot add CMake options: the
reference build that the tests verify against must be the build that a
reader of `test_case/` sees.

## Test types

`test_types` gives each test the dimension that it exercises. The runner
uses the first entry whose filter matches the test. Filters use positive
patterns only (`*`, `?`, `:`); an empty filter or a `-` pattern is an error.
A test that matches no entry is a `lib` test if the program has a library,
and a `bin` test if it does not.

The runner shows the type of each test in its live output (`[bin]`), in the
summary (counts per type), and in the JSON report (`test_type` of each
verdict, `program` with the given artifacts).

## Passing the artifacts

`harvest-bench run` takes `--lib` for the library and `--bin` for the
executable. With plain CMake, the suite takes the same artifacts as
variables: `TEST_LIB_PATH`, and for a program with an executable,
`TEST_BIN_PATH` (absolute path of the executable) and `TEST_BIN_NAME`
(`program.bin.name`). The suite stops at configure time if `TEST_BIN_PATH`
is given without `TEST_BIN_NAME`.

## Missing artifacts

If the program has both dimensions and you give only one artifact, the run
continues. The runner prints a warning, gives the suite a path that does not
exist for the missing artifact, and records every test of the missing type as
failed with the cause `missing_artifact`. A missing artifact never scores.
The JSON report lists it in `missing_artifacts`; `program` then has only
the artifacts that the run was given.

- Without `--bin`, the library tests run as usual. The executable tests fail;
  their output says that the placeholder executable cannot be started.
- Without `--lib`, the suite must still build. A suite that links the library
  at build time cannot, and the runner stops with the build error. A suite
  that loads the library at run time (`dlopen`) can run its executable
  tests.

An artifact that the program does not declare is an error, and so is a given
path that does not exist. `verify` and `gen-manifest` need every declared
artifact.

## Measuring budgets

Write the `program` block before you run `gen-manifest`, and give `--bin`
too. `gen-manifest` keeps the `program` and `test_types` of the existing
manifest; write the result with `--out`.

To write the executable tests themselves, see
[executable-tests.md](executable-tests.md).
