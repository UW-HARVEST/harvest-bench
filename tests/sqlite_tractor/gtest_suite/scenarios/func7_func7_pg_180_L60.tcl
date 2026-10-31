source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-180 {
  SELECT log10(1000.0)
} {3.0}
finish_test
