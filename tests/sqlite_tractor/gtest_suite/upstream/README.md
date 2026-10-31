# Provenance

SQLite source and tests: Git commit
`924626e603a36cc48ce87bc3a3eeddc61af1a72f`, Fossil check-in
`86bc7c8648fa6df303cd6953e377227d8a778b7becca984332bb6f623cb52cec`.
The development version string is 3.54.0; it does not identify a substitute
release. SQLite's source and tests are public domain; see LICENSE.md.

The `.test` files and their selected helper files are unchanged upstream files.
`test/tester.tcl` is the single entry-point redirect to `glue/tester.tcl`.
`glue/tester-procs.tcl` contains unchanged top-level procedure bodies extracted
from upstream `test/tester.tcl`, except the upstream startup, reset and final
private-testfixture checks, which the public harness replaces. The original
comparison procedure and floating-point comparator are preserved.

`tclsqlite.c`, `tclsqlite.h`, `tclext.h` and `sqlite3.h` are byte-identical to
DARPA Test-Corpus commit `cdbdcaac07d48b45e401d36ce68960d5b6ac1c58`,
`Public-Tests/P03_sqlite/sqlite/test_case`. These headers define the actual
DARPA ABI, including its changed signatures and removed constants.
The test-side Tcl wrapper manages its own connection object; SQLite connection
and statement objects remain opaque.

`test_session.c` and `test_recover.c` plus their public headers are also DARPA
sources. `glue/session.c` replaces private `sqlite3ErrStr` calls with public
`sqlite3_errstr`. `glue/recover.c` replaces the private include with standard
`string.h` and removes the private `sqlite3_dbdata_init` command entirely.
Only their public session/recover commands are initialized.

The optional FTS5 corruption marker in `glue/compare.c` is the unchanged
`f5tMayBeCorrupt` body from upstream `ext/fts5/fts5_tcl.c`. In this fixed
Release build its `SQLITE_DEBUG` body is compiled out, exactly as upstream;
it does not read or modify the engine. This permits upstream FTS5 common
setup to run without importing its private debug module.

`glue/public_bridge.c` extracts public-API test commands from upstream
`src/test1.c`, `test_blob.c`, `test_md5.c`, `test_hexio.c`, `test_mutex.c` and
`test_malloc.c`. It retains upstream pointer-map, BLOB, MD5, hex-file and
comparison helpers, with these changes:

- Replace private `sqlite3StrICmp`/`sqlite3ErrStr` with public counterparts.
- Omit the command using private `sqlite3Atoi64`, and the private
  `test_func.c` module (including record decode/extract helpers).
- Guard removed DARPA constants; use the fixed no-thread capability in the
  wrapper's own consistency check.
- Rename the wrapper's pointer helper to avoid its collision with tclsqlite.c.
- Rename the local error-name formatter copied from upstream `src/main.c`.
  This formatter reports constants; it is not another database implementation.

`scenario_manifest.json` gives source files, original test IDs and source
line ranges for literal SQL batches. Each copied fragment preserves its SQL
and expected result. Four identical batches are represented only once.
The adapted `scenarios/test_select1.tcl` also explicitly removes the three
locking-dependent cases select1-15.1/15.2/15.3 that the DARPA reference cannot
enter. Its raw upstream source remains unchanged for provenance.
Other files are scored as complete bounded stateful sequences. A sequence's
inner assertions are never registered as independent gtests. A failure aborts
that sequence and cannot poison another process.

`source_selection.json` lists the disposition of every audited leaf script.
It distinguishes included sequences, partially selected files, unsupported
harness requirements and excluded functionality/resources. Unselected parts
of split files are not covered. No expected result was changed to accept a
known DARPA failure. No official SQLite engine, system SQLite, CLI, or
standalone extension implementation is compiled into this suite.
