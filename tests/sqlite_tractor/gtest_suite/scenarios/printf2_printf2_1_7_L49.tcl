source $::suite_root/glue/tester.tcl
do_execsql_test printf2-1.7 {
  SELECT format('%.*f',2,3.141592653);
} {3.14}
finish_test
