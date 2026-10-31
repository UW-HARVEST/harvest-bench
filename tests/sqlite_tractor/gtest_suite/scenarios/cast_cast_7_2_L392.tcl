source $::suite_root/glue/tester.tcl
do_execsql_test cast-7.2 {
  SELECT CAST('-0' AS NUMERIC);
} {0}
finish_test
