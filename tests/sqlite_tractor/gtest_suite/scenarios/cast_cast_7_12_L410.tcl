source $::suite_root/glue/tester.tcl
do_execsql_test cast-7.12 {
  SELECT '' - 1;
} {-1}
finish_test
