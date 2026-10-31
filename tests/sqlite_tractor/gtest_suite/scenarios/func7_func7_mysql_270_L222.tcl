source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-270 {
   SELECT mod(34.5,3);
} {1.5}
finish_test
