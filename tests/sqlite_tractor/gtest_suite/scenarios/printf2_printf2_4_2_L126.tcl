source $::suite_root/glue/tester.tcl
do_execsql_test printf2-4.2 {
  SELECT printf('|%,d|%,d|',12,-12);
} {|12|-12|}
finish_test
