source $::suite_root/glue/tester.tcl
do_execsql_test date-6.25.6 {
  SELECT datetime('2000-10-29 12:00-00:00','utc','utc');
} {{2000-10-29 12:00:00}}
finish_test
