source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-120 {
  SELECT ceil(-42.2)
} {-42.0}
finish_test
