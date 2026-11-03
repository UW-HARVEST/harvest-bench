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
ifcapable vtab {
# Verify that generate_series stays sane near 64-bit range boundaries.
# See overflow report at https://sqlite.org/forum/forumpost/5d34ce5280
do_test shell2-1.4.10 {
 set res [catchcmd :memory: [string trim {
 .mode batch
 SELECT * FROM generate_series(9223372036854775807,9223372036854775807,1);
 SELECT * FROM generate_series(9223372036854775807,9223372036854775807,-1);
 SELECT avg(value),min(value),max(value) FROM generate_series(
  -9223372036854775808,9223372036854775807,1085102592571150095);
 SELECT * FROM generate_series(-9223372036854775808,9223372036854775807,
  9223372036854775807);
 SELECT value FROM generate_series(-4611686018427387904,
  4611686018427387904, 4611686018427387904) ORDER BY value DESC;
 SELECT * FROM generate_series(0,-2,-1);
 SELECT * FROM generate_series(0,-2);
 SELECT * FROM generate_series(0,2) LIMIT 3;}]]
} {0 {9223372036854775807
9223372036854775807
-0.5|-9223372036854775808|9223372036854775807
-9223372036854775808
-1
9223372036854775806
4611686018427387904
0
-4611686018427387904
0
-1
-2
0
1
2}}
}  ;# ifcapable vtab
finish_test
