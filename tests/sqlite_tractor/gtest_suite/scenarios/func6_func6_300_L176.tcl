source $::suite_root/glue/tester.tcl
do_execsql_test func6-300 {
  CREATE TABLE t2(a INT, b INT PRIMARY KEY) WITHOUT ROWID;
  CREATE INDEX x3 ON t2(b);
  CREATE TABLE t1(a INT PRIMARY KEY, b TEXT);
  SELECT * FROM t1 WHERE a IN (SELECT sqlite_offset(b) FROM t2);
} {}
finish_test
