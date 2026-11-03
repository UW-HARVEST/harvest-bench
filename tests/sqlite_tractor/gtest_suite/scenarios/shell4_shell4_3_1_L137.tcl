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

do_test shell4-3.1 {
  set fd [open t1.txt wb]
  puts $fd ".mode list\nSELECT 'squirrel';"
  close $fd
  exec $::CLI_ONLY --noinit :memory: --interactive ".read t1.txt"
} {squirrel}
do_test_with_ansi_output shell4-3.2 {
  set fd [open t1.txt wb]
  puts $fd ".mode list\nSELECT 'pound: \302\243';"
  close $fd
  exec $::CLI_ONLY --noinit :memory: --interactive ".read t1.txt"
} {pound: £}

do_test shell4-4.1 {
  set fd [open t1.txt wb]
  puts $fd ".read t1.txt"
  close $fd
  catchcmd ":memory:" ".read t1.txt"
} {1 {t1.txt: Input nesting limit (25) reached at line 1. Check recursion.}}

finish_test
