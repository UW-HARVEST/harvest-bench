source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-200 {
   SELECT floor(1.23), floor(-1.23);
} {1.0 -2.0}
finish_test
