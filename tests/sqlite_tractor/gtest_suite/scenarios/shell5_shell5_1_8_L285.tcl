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
if {$tcl_platform(platform)=="unix"} {
  do_test shell5-1.8 {
    forcedelete test.db
    catchcmd test.db {.mode csv
.import "|awk 'END{print \"x,y\";for(i=1;i<=5;i++){print i \",this is \" i}}'" t1
SELECT * FROM t1;}
  } {0 {1,"this is 1"
2,"this is 2"
3,"this is 3"
4,"this is 4"
5,"this is 5"}}
}
finish_test
