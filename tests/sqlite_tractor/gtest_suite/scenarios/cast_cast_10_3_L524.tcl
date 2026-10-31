source $::suite_root/glue/tester.tcl
do_execsql_test cast-10.3 {
  SELECT * FROM (VALUES(CAST(44 AS REAL)),(55));
} {44.0 55}
finish_test
