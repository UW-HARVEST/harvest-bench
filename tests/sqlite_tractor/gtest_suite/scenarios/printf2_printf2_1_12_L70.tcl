source $::suite_root/glue/tester.tcl
do_execsql_test printf2-1.12 {
  SELECT format('%.*z',5,'abcdefghijklmnop');
} {abcde}
finish_test
