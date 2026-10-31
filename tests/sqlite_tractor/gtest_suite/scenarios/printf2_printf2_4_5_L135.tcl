source $::suite_root/glue/tester.tcl
do_execsql_test printf2-4.5 {
  SELECT printf('|%,d|%,d|',12345,-12345);
} {|12,345|-12,345|}
finish_test
