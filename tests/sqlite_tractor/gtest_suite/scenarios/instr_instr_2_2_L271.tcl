source $::suite_root/glue/tester.tcl
do_execsql_test instr-2.2 {
  DROP TABLE IF EXISTS t1;
  CREATE TABLE t1(x);
  INSERT INTO t1(x) VALUES('text'),(x'bb');
  SELECT quote(x) FROM t1 WHERE instr(x'aabb',x);
} {X'BB'}
finish_test
