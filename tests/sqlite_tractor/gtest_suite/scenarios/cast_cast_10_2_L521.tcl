source $::suite_root/glue/tester.tcl
do_execsql_test cast-10.2 {
  SELECT CAST(44 AS REAL) AS 'm' UNION ALL SELECT 55;
} {44.0 55}
finish_test
