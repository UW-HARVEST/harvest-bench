source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-150 {
   SELECT round( atan(2), 7), round( atan(-2), 7);
} {1.1071487 -1.1071487}
finish_test
