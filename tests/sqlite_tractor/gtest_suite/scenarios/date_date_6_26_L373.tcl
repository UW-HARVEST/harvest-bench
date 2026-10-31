source $::suite_root/glue/tester.tcl
do_execsql_test date-6.26 {
  SELECT datetime('2000-10-29 12:00:00+05:00');
} {{2000-10-29 07:00:00}}
finish_test
