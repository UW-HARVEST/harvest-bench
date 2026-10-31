source $::suite_root/glue/tester.tcl
do_execsql_test cast-9.0 {
  CREATE TABLE t0(c0);
  INSERT INTO t0(c0) VALUES (0);
  CREATE VIEW v1(c0, c1) AS 
    SELECT CAST(0.0 AS NUMERIC), COUNT(*) OVER () FROM t0;
  SELECT v1.c0 FROM v1, t0 WHERE v1.c0=0; 
} {0.0}
finish_test
