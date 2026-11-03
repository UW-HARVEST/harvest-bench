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

# Test with echo on using dot command and 
# turning off mid- processing.
# NB. whitespace is important
do_test shell2-1.4.4 {
  forcedelete foo.db
  catchcmd "foo.db" {
.mode batch
.echo ON
CREATE TABLE foo(a);
.echo OFF
INSERT INTO foo(a) VALUES(1);
SELECT * FROM foo;}
} {0 {CREATE TABLE foo(a);
.echo OFF
1}}
finish_test
