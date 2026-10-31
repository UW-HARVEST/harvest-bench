source $::suite_root/glue/tester.tcl
do_execsql_test instr-2.0 {
  DROP TABLE IF EXISTS t0;
  CREATE TABLE t0(c0 PRIMARY KEY, c1);
  INSERT INTO t0(c0) VALUES (x'bb'), (0);
  SELECT COUNT(*) FROM t0 WHERE INSTR(x'aabb', t0.c0) ORDER BY t0.c0, t0.c1;
} {1}
finish_test
