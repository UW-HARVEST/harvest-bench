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

# 2024-03-11 https://sqlite.org/forum/forumpost/ca014d7358
# Import into a table that contains computed columns.
#
do_test shell5-7.1 {
  set out [open shell5.csv w]
  fconfigure $out -translation lf
  puts $out {aaa|bbb}
  close $out
  forcedelete test.db
  catchcmd ":memory: -list" {CREATE TABLE t1(a TEXT, b TEXT, c AS (a||b));
.import shell5.csv t1
SELECT * FROM t1;}
} {0 aaa|bbb|aaabbb}

#-------------------------------------------------------------------------

do_test shell5-8.1 {

  set out [open shell5.csv w]
  fconfigure $out -translation lf
  puts $out x
  close $out

  catchcmd :memory: {.import --csv shell5.csv '""""""""""""""""""""""""""""""""""""""""""""""'}
} {0 {}}
finish_test
