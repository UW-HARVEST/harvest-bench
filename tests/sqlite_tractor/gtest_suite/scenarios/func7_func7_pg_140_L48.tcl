source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-140 {
  SELECT floor(42.8)
} {42.0}
finish_test
