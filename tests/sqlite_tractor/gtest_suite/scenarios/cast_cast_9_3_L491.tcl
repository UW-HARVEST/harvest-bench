source $::suite_root/glue/tester.tcl
do_execsql_test cast-9.3 {
  SELECT CAST(4.5 AS NUMERIC);
} {4.5}
finish_test
