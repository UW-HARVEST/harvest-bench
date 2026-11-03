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

#----------------------------------------------------------------------------
# Test cases shell5-1.*: Basic handling of the .import and .separator commands.
#

# .import FILE TABLE     Import data from FILE into TABLE
do_test shell5-1.1.1 {
  catchcmd "test.db" ".import"
} {/1 .line 1: Missing FILE argument.*/}
finish_test
