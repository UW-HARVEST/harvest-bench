# 2009 Nov 11
#
# The author disclaims copyright to this source code.  In place of
# a legal notice, here is a blessing:
#
#    May you do good and not evil.
#    May you find forgiveness for yourself and forgive others.
#    May you share freely, never taking more than you give.
#
#***********************************************************************
#
# The focus of this file is testing the CLI shell tool.
#
# TESTRUNNER: shell
#

# Test plan:
#
#   shell1-1.*: Basic command line option handling.
#   shell1-2.*: Basic "dot" command token parsing.
#   shell1-3.*: Basic test that "dot" command can be called.
#   shell1-{4-8}.*: Test various "dot" commands's functionality.
#   shell1-9.*: Basic test that "dot" commands and SQL intermix ok.
#
set testdir $::suite_root/upstream/test
source $testdir/tester.tcl
set CLI [test_cli_invocation]
db close
forcedelete test.db test.db-journal test.db-wal
sqlite3 db test.db

# -init filename       read/process named file
forcedelete FOO
set out [open FOO w]
puts $out ""
close $out
do_test shell1-1.3.1 {
  catchcmd "-init FOO test.db" ""
} {0 {}}
do_test shell1-1.3.2 {
  catchcmd "-init FOO test.db .quit BAD" ""
} {0 {}}
do_test shell1-1.3.3 {
  catchcmd "-init FOO test.db BAD .quit" ""
} {/1 .Parse error in 4th command line argument: near "BAD": syntax error*/}

# -echo                print commands before execution
do_test shell1-1.4.1 {
  catchcmd "-echo test.db" ""
} {0 {}}

# -[no]header          turn headers on or off
do_test shell1-1.5.1 {
  catchcmd "-header test.db" ""
} {0 {}}
do_test shell1-1.5.2 {
  catchcmd "-noheader test.db" ""
} {0 {}}

# -bail                stop after hitting an error
do_test shell1-1.6.1 {
  catchcmd "-bail test.db" ""
} {0 {}}
finish_test
