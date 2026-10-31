source $::suite_root/glue/tester.tcl
do_execsql_test printf2-4.3 {
  SELECT printf('|%,d|%,d|',123,-123);
} {|123|-123|}
finish_test
