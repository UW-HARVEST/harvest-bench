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
#
# The focus of this file is testing the CLI shell tool.
#
# TESTRUNNER: shell
#

# Test plan:
#
#   shell1-1.*: Basic command line option handling.
#   shell1-2.*: Basic "dot" command token parsing.
#   shell1-3.*: Basic test that "dot" command can be called.
#   shell1-{4-8}.*: Test various "dot" commands's functionality.
#   shell1-9.*: Basic test that "dot" commands and SQL intermix ok.
#
set testdir $::suite_root/upstream/test
source $testdir/tester.tcl
set CLI [test_cli_invocation]
db close
forcedelete test.db test.db-journal test.db-wal
sqlite3 db test.db

# Test case for the ieee754 and decimal extensions in the shell.
# See the "floatingpoint.html" file in the documentation for more
# information.
#
do_test shell1-8.1 {
  catchcmd ":memory:" {.mode batch
    -- The pow2 table will hold all the necessary powers of two.
    CREATE TABLE pow2(x INTEGER PRIMARY KEY, v TEXT);
    WITH RECURSIVE c(x,v) AS (
      VALUES(0,'1')
      UNION ALL
      SELECT x+1, decimal_mul(v,'2') FROM c WHERE x+1<=971
    ) INSERT INTO pow2(x,v) SELECT x, v FROM c;
    WITH RECURSIVE c(x,v) AS (
      VALUES(-1,'0.5')
      UNION ALL
      SELECT x-1, decimal_mul(v,'0.5') FROM c WHERE x-1>=-1075
    ) INSERT INTO pow2(x,v) SELECT x, v FROM c;
    
    -- This query finds the decimal representation of each value in the "c" table.
    WITH c(n) AS (VALUES(47.49))
                     ----XXXXX----------- Replace with whatever you want
    SELECT decimal_mul(ieee754_mantissa(c.n),pow2.v)
      FROM pow2, c WHERE pow2.x=ieee754_exponent(c.n);
  }
} {0 47.49000000000000198951966012828052043914794921875}
finish_test
