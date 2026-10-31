source $::suite_root/glue/tester.tcl
do_execsql_test printf2-1.8 {
  SELECT printf('%*.*f',5,2,3.141592653);
} {{ 3.14}}
finish_test
