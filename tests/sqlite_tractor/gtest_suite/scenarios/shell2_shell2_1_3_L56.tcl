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

# Test a problem reported on the mailing list. The shell was at one point
# returning the generic SQLITE_ERROR message ("SQL error or missing database")
# instead of the "triggers nested too deep" message in the test below.
#
do_test shell2-1.3 {
  catchcmd "-batch test.db" {
    PRAGMA recursive_triggers = ON;
    CREATE TABLE t5(a PRIMARY KEY, b, c);
    INSERT INTO t5 VALUES(1, 2, 3);
    CREATE TRIGGER au_tble AFTER UPDATE ON t5 BEGIN
      UPDATE OR IGNORE t5 SET a = new.a, c = 10;
    END;

    UPDATE OR REPLACE t5 SET a = 4 WHERE a = 1;
  }
} {1 {Error near line 9: triggers nested too deep}}
finish_test
