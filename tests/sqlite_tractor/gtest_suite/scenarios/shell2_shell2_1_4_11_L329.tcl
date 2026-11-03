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
# TESTRUNNER: shell
#
# The focus of this file is testing the CLI shell tool.
#
# $Id: shell2.test,v 1.7 2009/07/17 16:54:48 shaneh Exp $
#

# Test plan:
#
#   shell2-1.*: Misc. test of various tickets and reported errors.
#
set testdir $::suite_root/upstream/test
source $testdir/tester.tcl
set CLI [test_find_cli]
db close
forcedelete test.db test.db-journal test.db-wal
sqlite3 db test.db
ifcapable vtab {
# Bug discovered while messing around, .import hangs with
# bit 7 set in column separator.
do_test shell2-1.4.11 {
  forcedelete dummy.csv
  set df [open dummy.csv w]
  puts $df dog,cat
  close $df
  set res [catchcmd :memory: [string trim {
 CREATE TABLE t(line text);
.mode ascii -colsep "\377" -rowsep "\n"
.import dummy.csv t
 SELECT count(*) FROM t;}]]
} {1 {0
Error: .import column separator must be ASCII}}
} ;# ifcapable vtab
finish_test
