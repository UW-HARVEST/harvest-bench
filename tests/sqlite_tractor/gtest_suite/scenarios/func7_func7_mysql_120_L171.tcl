source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-120 {
   SELECT round( acos(0.0), 7);
} {1.5707963}
finish_test
