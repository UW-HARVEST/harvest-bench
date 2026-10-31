source $::suite_root/glue/tester.tcl
do_execsql_test func3-5.33 {
  SELECT unlikely(NULL);
} {{}}
finish_test
