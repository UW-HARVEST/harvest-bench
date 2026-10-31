source $::suite_root/glue/tester.tcl
do_execsql_test cast-7.20 {
  DROP TABLE IF EXISTS t0;
  CREATE TABLE t0 (c0 TEXT);
  INSERT INTO t0(c0) VALUES ('1.0');
  SELECT CAST(c0 AS NUMERIC) FROM t0;
} {1}
finish_test
