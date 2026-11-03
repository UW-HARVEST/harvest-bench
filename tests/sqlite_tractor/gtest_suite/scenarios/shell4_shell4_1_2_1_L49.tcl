# 2010 July 28
#
# The author disclaims copyright to this source code.  In place of
# a legal notice, here is a blessing:
#
#    May you do good and not evil.
#    May you find forgiveness for yourself and forgive others.
#    May you share freely, never taking more than you give.
#
#***********************************************************************
# TESTRUNNER: shell
#
# The focus of this file is testing the CLI shell tool.
# These tests are specific to the .stats command.
#
# 2015-03-19:  Added tests for .trace

# Test plan:
#
#   shell4-1.*: Basic tests specific to the "stats" command.
#   shell4-2.*: Basic tests for ".trace"
#   shell4-3.*: The ".read" command takes the shell out of interactive mode
#   shell4-4.*: Input redirects cannot recurse too much
#
set testdir $::suite_root/upstream/test
source $testdir/tester.tcl
set CLI [test_cli_invocation]
set CLI_ONLY [test_find_cli]
db close
forcedelete test.db test.db-journal test.db-wal
sqlite3 db test.db

# -stats should turn it on
do_test shell4-1.2.1 {
  set res [catchcmd "-stats test.db" ".show"]
  list [regexp {stats: on} $res]
} {1}

do_test shell4-1.2.2 {
  set res [catchcmd "-stats test.db" ".show"]
  list [regexp {stats: off} $res]
} {0}

# .stats ON|OFF          Turn stats on or off
#do_test shell4-1.3.1 {
#  catchcmd "test.db" ".stats"
#} {1 {Usage: .stats on|off}}
do_test shell4-1.3.2 {
  catchcmd "test.db" ".stats ON"
} {0 {}}
do_test shell4-1.3.3 {
  catchcmd "test.db" ".stats OFF"
} {0 {}}
finish_test
