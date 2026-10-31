source $::suite_root/glue/tester.tcl
do_execsql_test expr-14.1 {
  DROP TABLE IF EXISTS t1;
  CREATE TABLE t1(x);
  INSERT INTO t1 VALUES(0),(1),(NULL),(0.5),('1x'),('0x');
  SELECT count(*) FROM t1
   WHERE (x OR (8==9)) != (CASE WHEN x THEN 1 ELSE 0 END);
} {0}
finish_test
