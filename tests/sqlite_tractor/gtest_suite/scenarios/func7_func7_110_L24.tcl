source $::suite_root/glue/tester.tcl
do_execsql_test func7-110 {
  SELECT quote(ceil(NULL)), ceil('-99.99');
} {NULL -99.0}
finish_test
