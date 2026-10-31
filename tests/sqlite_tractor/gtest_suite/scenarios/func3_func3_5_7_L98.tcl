source $::suite_root/glue/tester.tcl
do_execsql_test func3-5.7 {
  SELECT likelihood(123, 1.0), likelihood(456, 0.0);
} {123 456}
finish_test
