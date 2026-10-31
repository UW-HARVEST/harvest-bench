source $::suite_root/glue/tester.tcl
do_execsql_test printf2-4.10 {
  SELECT printf('|%,d|%,d|',1234567890,-1234567890);
} {|1,234,567,890|-1,234,567,890|}
finish_test
