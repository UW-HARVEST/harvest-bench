source $::suite_root/glue/tester.tcl
do_execsql_test func3-5.32 {
  SELECT unlikely(14.125);
} {14.125}
finish_test
