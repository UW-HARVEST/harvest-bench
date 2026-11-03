# Provenance

SQLite source and tests: Git commit
`924626e603a36cc48ce87bc3a3eeddc61af1a72f`, Fossil check-in
`86bc7c8648fa6df303cd6953e377227d8a778b7becca984332bb6f623cb52cec`.
The development version string is 3.54.0; it does not identify a substitute
release. SQLite's source and tests are public domain; see LICENSE.md.

The `.test`, `.tcl` and `.sql` files under `test/` and `ext/` are unchanged
upstream files. `test/tester.tcl` is the single entry-point redirect to
`glue/tester.tcl`. `glue/tester-procs.tcl` contains unchanged top-level
procedure bodies extracted from upstream `test/tester.tcl`, except the
upstream startup, reset and final private-testfixture checks, which the
public harness replaces. `test_find_cli` and `test_cli_invocation` look up
`$::harvest_bin_name` (the manifest's `program.bin.name`) instead of the
constant `sqlite3`.

`tclsqlite.c`, `tclsqlite.h`, `tclext.h`, `sqlite3.h`, `sqlite3recover.h` and
`sqlite3session.h` are byte-identical to DARPA Test-Corpus commit
`cdbdcaac07d48b45e401d36ce68960d5b6ac1c58`,
`Public-Tests/P03_sqlite/sqlite/test_case`. They define the actual DARPA ABI,
including its changed signatures and removed constants.

`glue/session.c` and `glue/recover.c` are the DARPA `test_session.c` and
`test_recover.c`. `session.c` replaces private `sqlite3ErrStr` calls with
public `sqlite3_errstr`. `recover.c` replaces the private include with
`string.h` and removes the private `sqlite3_dbdata_init` command. Only their
public session/recover commands are initialized.

The optional FTS5 corruption marker in `glue/compare.c` is the unchanged
`f5tMayBeCorrupt` body from upstream `ext/fts5/fts5_tcl.c`. In this Release
build its `SQLITE_DEBUG` body is compiled out, as upstream; it does not touch
the engine.

`glue/public_bridge.c` extracts public-API test commands from upstream
`src/test1.c`, `test_blob.c`, `test_md5.c`, `test_hexio.c`, `test_mutex.c` and
`test_malloc.c`, with these changes:

- Replace private `sqlite3StrICmp`/`sqlite3ErrStr` with public counterparts.
- Omit the command using private `sqlite3Atoi64`, and the private
  `test_func.c` module (including record decode/extract helpers).
- Guard removed DARPA constants; use the fixed no-thread capability in the
  wrapper's own consistency check.
- Rename the wrapper's pointer helper to avoid its collision with tclsqlite.c.
- Rename the local error-name formatter copied from upstream `src/main.c`.

Files under `scenarios/` copy selected lines of upstream files; their names
give the source file, first test ID and line. No expected result was changed.
No official SQLite engine, system SQLite, CLI or extension implementation is
compiled into this suite.
