source $::suite_root/glue/tester.tcl
do_execsql_test printf2-1.9 {
  SELECT format('%d',314159.2653);
} {314159}
finish_test
