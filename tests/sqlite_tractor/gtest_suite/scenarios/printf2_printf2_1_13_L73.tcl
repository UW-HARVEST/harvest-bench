source $::suite_root/glue/tester.tcl
do_execsql_test printf2-1.13 {
  SELECT printf('%c','abcdefghijklmnop');
} {a}
finish_test
