source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-110 {
  SELECT ceil(42.2)
} {43.0}
finish_test
