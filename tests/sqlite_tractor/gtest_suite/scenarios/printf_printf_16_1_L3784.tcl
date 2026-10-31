source $::suite_root/glue/tester.tcl
do_execsql_test printf-16.1 {
  SELECT printf('%.*g',2147483647,0.01);
} {0.01}
finish_test
