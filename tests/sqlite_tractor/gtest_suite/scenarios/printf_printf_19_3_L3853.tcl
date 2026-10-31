source $::suite_root/glue/tester.tcl
do_execsql_test printf-19.3 {
  SELECT format('%,.0f %,.0f',12345e+10, 12345e+11);
} {{123,450,000,000,000 1,234,500,000,000,000}}
finish_test
