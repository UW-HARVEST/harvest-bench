source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-180 {
   SELECT cos(pi());
} {-1.0}
finish_test
