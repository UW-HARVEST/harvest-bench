source $::suite_root/glue/tester.tcl
do_execsql_test printf2-4.4 {
  SELECT printf('|%,d|%,d|',1234,-1234);
} {|1,234|-1,234|}
finish_test
