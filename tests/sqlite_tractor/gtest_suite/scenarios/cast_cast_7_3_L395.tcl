source $::suite_root/glue/tester.tcl
do_execsql_test cast-7.3 {
  SELECT CAST('+' AS NUMERIC);
} {0}
finish_test
