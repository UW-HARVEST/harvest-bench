source $::suite_root/glue/tester.tcl
do_execsql_test printf2-1.2 {
  SELECT printf('hello');
} {hello}
finish_test
