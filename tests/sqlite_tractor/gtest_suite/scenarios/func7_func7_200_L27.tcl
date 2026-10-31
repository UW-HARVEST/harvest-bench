source $::suite_root/glue/tester.tcl
do_execsql_test func7-200 {
  SELECT round(ln(5),2), log(100.0), log(100), log(2,'256');
} {1.61 2.0 2.0 8.0}
finish_test
