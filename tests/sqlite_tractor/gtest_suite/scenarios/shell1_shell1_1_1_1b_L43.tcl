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
do_test shell1-1.1.1b {
  set res [catchcmd "test.db -bad" ""]
  set rc [lindex $res 0]
  list $rc \
       [regexp {Error: unknown option: -bad} $res]
} {1 1}
finish_test
