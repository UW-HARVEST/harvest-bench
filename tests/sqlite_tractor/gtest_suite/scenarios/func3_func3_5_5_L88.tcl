source $::suite_root/glue/tester.tcl
do_execsql_test func3-5.5 {
  SELECT likelihood('test-string', 0.5);
} {test-string}
finish_test
