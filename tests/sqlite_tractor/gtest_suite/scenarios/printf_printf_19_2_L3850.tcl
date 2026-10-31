source $::suite_root/glue/tester.tcl
do_execsql_test printf-19.2 {
  SELECT format('%0.0f %#0.0f',0.0, 0.0);
} {{0 0.}}
finish_test
