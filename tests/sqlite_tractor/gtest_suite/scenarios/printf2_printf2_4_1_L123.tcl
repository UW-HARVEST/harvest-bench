source $::suite_root/glue/tester.tcl
do_execsql_test printf2-4.1 {
  SELECT printf('|%,d|%,d|',0,-1);
} {|0|-1|}
finish_test
