source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-130 {
   SELECT round( asin(0.2), 7);
} {0.2013579}
finish_test
