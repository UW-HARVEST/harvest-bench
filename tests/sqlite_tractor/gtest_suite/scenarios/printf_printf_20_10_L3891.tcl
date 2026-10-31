source $::suite_root/glue/tester.tcl
do_execsql_test printf-20.10 {
  SELECT format('1-%j-2-%J-3',null,null);
} {1--2-null-3}
finish_test
