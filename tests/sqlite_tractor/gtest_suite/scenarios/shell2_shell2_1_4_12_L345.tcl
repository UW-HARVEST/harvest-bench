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

# Bug from forum post 7cbe081746dd3803
# Keywords as column names were producing an error message.
do_test shell2-1.4.12 {
  set res [catchcmd :memory: [string trim {.mode batch
 CREATE TABLE "group"("order" text);
 INSERT INTO "group" VALUES ('ABC');
.sha3sum}]]
} {0 ca08bc02b7e95c7df431a3a4b1cc0f8d8743914793473f55b5558e03}
finish_test
