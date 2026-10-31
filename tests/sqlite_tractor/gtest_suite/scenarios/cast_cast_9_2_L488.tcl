source $::suite_root/glue/tester.tcl
do_execsql_test cast-9.2 {
  SELECT CAST(4.0 AS NUMERIC);
} {4.0}
finish_test
