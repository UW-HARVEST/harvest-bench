source $::suite_root/glue/tester.tcl
do_execsql_test printf2-1.11 {
  SELECT format('%lld%n',314159.2653,'hi');
} {314159}
finish_test
