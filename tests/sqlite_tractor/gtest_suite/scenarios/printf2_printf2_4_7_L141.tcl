source $::suite_root/glue/tester.tcl
do_execsql_test printf2-4.7 {
  SELECT printf('|%,d|%,d|',1234567,-1234567);
} {|1,234,567|-1,234,567|}
finish_test
