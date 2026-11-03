# 2010 August 4
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
# These tests are specific to the .import command.
#
# $Id: shell5.test,v 1.7 2009/07/17 16:54:48 shaneh Exp $
#

# Test plan:
#
#   shell5-1.*: Basic tests specific to the ".import" command.
#
set testdir $::suite_root/upstream/test
source $testdir/tester.tcl
set CLI [test_cli_invocation]
db close
forcedelete test.db test.db-journal test.db-wal

do_test_with_ansi_output shell5-6.2 {
  set out [open shell5.csv w]
  fconfigure $out -translation lf
  puts $out {1,2}
  puts $out {あい,うえお}
  close $out
  forcedelete test.db
  catchcmd test.db {.import -csv shell5.csv t1
.mode line
SELECT * FROM t1;}
} {0 {1: あい
2: うえお}}
finish_test
