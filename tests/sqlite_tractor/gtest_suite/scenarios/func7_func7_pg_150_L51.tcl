source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-150 {
  SELECT floor(-42.8)
} {-43.0}
finish_test
