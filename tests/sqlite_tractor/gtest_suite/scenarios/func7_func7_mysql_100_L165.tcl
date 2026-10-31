source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-100 {
   SELECT acos(1);
} {0.0}
finish_test
