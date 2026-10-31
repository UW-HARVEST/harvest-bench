source $::suite_root/glue/tester.tcl
do_execsql_test func3-5.52 {
  SELECT likely(14.125);
} {14.125}
finish_test
