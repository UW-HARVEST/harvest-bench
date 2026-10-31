source $::suite_root/glue/tester.tcl
do_execsql_test printf2-4.6 {
  SELECT printf('|%,d|%,d|',123456,-123456);
} {|123,456|-123,456|}
finish_test
