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

# Verify that safe mode rejects certain UDFs
# Reported at https://sqlite.org/forum/forumpost/07beac8056151b2f
do_test shell2-1.4.8 {
  catchcmd "-safe :memory:" {
 SELECT edit('DoNotCare');}
} {1 {line 2: cannot use the edit() function in safe mode}}
finish_test
