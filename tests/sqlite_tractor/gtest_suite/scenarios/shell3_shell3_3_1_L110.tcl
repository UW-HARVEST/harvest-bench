# 2009 Dec 16
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
#   shell3-1.*: Basic tests for running SQL statments from command line.
#   shell3-2.*: Basic tests for running SQL file from command line.
#   shell3-3.*: Basic tests for processing odd SQL constructs.
#
set testdir $::suite_root/upstream/test
source $testdir/tester.tcl
set CLI [test_cli_invocation]
db close
forcedelete test.db test.db-journal test.db-wal
sqlite3 db test.db


# There are inconsistencies in command-line argument quoting on Windows.
# In particular, individual applications are responsible for command-line
# parsing in Windows, not the shell.  Depending on whether the sqlite3.exe
# program is compiled with MinGW or MSVC, the command-line parsing is
# different.  This causes problems for the tests below.  To avoid
# issues, these tests are disabled for windows.
#
if {$::tcl_platform(platform) eq "windows"} {
  finish_test
  return
}


#----------------------------------------------------------------------------
#   shell3-3.*: Basic tests for processing odd SQL constructs.
#

# Run combinations of odd identifiers, comments, semicolon placement
do_test shell3-3.1 {
  forcedelete foo.db
  set rc [ catchcmd "foo.db" {CREATE TABLE t1("
a--.
" --x
); CREATE TABLE t2("a[""b""]");
.header on
INSERT INTO t1 VALUES ('
x''y');
INSERT INTO t2 VALUES ('
/*.
.*/ x
''y');
SELECT * from t1 limit 1;
SELECT * from t2 limit 1;
} ]
  set fexist [file exist foo.db]
  list $rc $fexist
} {{0 {
a--.


x'y
a["b"]

/*.
.*/ x
'y}} 1}
finish_test
