source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-281 {
   SELECT power(2,2), power(2,-2);
} {4.0 0.25}
finish_test
