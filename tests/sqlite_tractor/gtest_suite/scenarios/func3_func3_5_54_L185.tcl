source $::suite_root/glue/tester.tcl
do_execsql_test func3-5.54 {
  SELECT likely('test-string');
} {test-string}
finish_test
