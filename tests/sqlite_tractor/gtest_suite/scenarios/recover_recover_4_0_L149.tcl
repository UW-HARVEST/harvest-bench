# 2019 April 23
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
# Test the shell tool ".ar" command.
#

set testdir $::suite_root/upstream/test
source $testdir/tester.tcl
set testprefix recover

ifcapable !vtab {
  finish_test; return
}
set CLI [test_find_cli]

proc compare_result {db1 db2 sql} {
  set r1 [$db1 eval $sql]
  set r2 [$db2 eval $sql]
  if {$r1 != $r2} {
  puts "r1: $r1"
  puts "r2: $r2"
    error "mismatch for $sql"
  }
  return ""
}

proc compare_dbs {db1 db2} {
  compare_result $db1 $db2 "SELECT sql FROM sqlite_master ORDER BY 1"
  foreach tbl [$db1 eval {SELECT name FROM sqlite_master WHERE type='table'}] {
    compare_result $db1 $db2 "SELECT * FROM $tbl"
  }
}

proc recover_with_opts {opts} {
  set cmd ".recover $opts"
  set fd [open [list |$::CLI -noinit test.db $cmd]]
  fconfigure $fd -translation binary
  set sql [read $fd]
  close $fd

  # Remove the ".dbconfig defensive off" line
  set sql [string map {".dbconfig defensive off" ""} $sql]

  forcedelete test.db2
  sqlite3 db2 test.db2
  execsql $sql db2
  db2 close
}

proc do_recover_test {tn {tsql {}} {res {}}} {
  recover_with_opts ""

  sqlite3 db2 test.db2
  if {$tsql==""} {
    uplevel [list do_test $tn [list compare_dbs db db2] {}]
  } else {
    uplevel [list do_execsql_test -db db2 $tn $tsql $res]
  }
  db2 close
}

set doc {
  hello
  world
}

#-------------------------------------------------------------------------
reset_db 
execsql { PRAGMA secure_delete = 0 }
execsql { PRAGMA auto_vacuum = 0 }
do_execsql_test 4.0 {
  CREATE TABLE t1(a, b, c);
  CREATE TABLE t2(d, e, f);
  CREATE TABLE t3(g, h, i);

  INSERT INTO t2 VALUES(1, 2, 3);
  INSERT INTO t2 VALUES('a', 'b', 'c');

  INSERT INTO t3 VALUES('one', 'two', 'three');
  DROP TABLE t1;
  DROP TABLE t2;
}

recover_with_opts ""
sqlite3 db2 test.db2
do_execsql_test -db db2 4.1.1 {
  SELECT name FROM sqlite_schema
} {t3 lost_and_found}
do_execsql_test -db db2 4.1.2 {
  SELECT id, c0, c1, c2 FROM lost_and_found
} {1 1 2 3    2 a b c}
db2 close

recover_with_opts -ignore-freelist
sqlite3 db2 test.db2
do_execsql_test -db db2 4.2.1 {
  SELECT name FROM sqlite_schema
} {t3}
do_execsql_test -db db2 4.2.2 {
  SELECT * FROM t3
} {one two three}
db2 close

finish_test
