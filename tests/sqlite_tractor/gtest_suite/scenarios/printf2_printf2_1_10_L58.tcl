source $::suite_root/glue/tester.tcl
do_execsql_test printf2-1.10 {
  SELECT printf('%lld',314159.2653);
} {314159}
finish_test
