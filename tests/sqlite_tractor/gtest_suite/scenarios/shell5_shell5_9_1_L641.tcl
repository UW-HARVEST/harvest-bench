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


# 2025-12-29 https://sqlite.org/forum/forumpost/6c1c0e213d
# .import honor .bail
#
do_test shell5-9.1 {
  catchcmd ":memory:" {
    CREATE TABLE t1(a,b,c INT CHECK(c<>5));
.bail on
.import -csv <<END t1
1,2,3
"a","b","c"
3,4,5
"q","r","s"
END
SELECT * FROM t1;}
} {1 {<stdin>:7: INSERT failed: CHECK constraint failed: c<>5}}
finish_test
