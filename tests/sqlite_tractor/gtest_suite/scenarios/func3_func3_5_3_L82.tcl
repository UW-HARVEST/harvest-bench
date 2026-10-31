source $::suite_root/glue/tester.tcl
do_execsql_test func3-5.3 {
  SELECT likelihood(14.125, 0.5);
} {14.125}
finish_test
