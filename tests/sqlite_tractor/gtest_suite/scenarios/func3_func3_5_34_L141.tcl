source $::suite_root/glue/tester.tcl
do_execsql_test func3-5.34 {
  SELECT unlikely('test-string');
} {test-string}
finish_test
