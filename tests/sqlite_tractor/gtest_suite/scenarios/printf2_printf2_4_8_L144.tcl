source $::suite_root/glue/tester.tcl
do_execsql_test printf2-4.8 {
  SELECT printf('|%,d|%,d|',12345678,-12345678);
} {|12,345,678|-12,345,678|}
finish_test
