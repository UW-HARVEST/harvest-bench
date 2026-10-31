source $::suite_root/glue/tester.tcl
do_execsql_test printf2-1.1 {
  SELECT quote(format()), quote(format(NULL,1,2,3));
} {NULL NULL}
finish_test
