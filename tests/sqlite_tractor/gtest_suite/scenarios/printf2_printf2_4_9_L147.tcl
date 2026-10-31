source $::suite_root/glue/tester.tcl
do_execsql_test printf2-4.9 {
  SELECT printf('|%,d|%,d|',123456789,-123456789);
} {|123,456,789|-123,456,789|}
finish_test
