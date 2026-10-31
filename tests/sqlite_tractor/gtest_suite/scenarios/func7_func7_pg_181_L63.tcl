source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-181 {
  SELECT format('%.30f', log10(100.0) );
} {2.000000000000000000000000000000}
finish_test
