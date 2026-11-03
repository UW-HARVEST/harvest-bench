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


#----------------------------------------------------------------------------
#   shell2-1.*: Misc. test of various tickets and reported errors.
#

# Batch mode not creating databases.  
# Reported on mailing list by Ken Zalewski.
# Ticket [aeff892c57].
do_test shell2-1.1.1 {
  forcedelete foo.db
  set rc [ catchcmd "-batch foo.db" "CREATE TABLE t1(a);" ]
  set fexist [file exist foo.db]
  list $rc $fexist
} {{0 {}} 1}
finish_test
