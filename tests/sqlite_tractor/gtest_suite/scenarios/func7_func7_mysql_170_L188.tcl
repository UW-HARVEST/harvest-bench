source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-170 {
   SELECT ceiling(1.23), ceiling(-1.23);
} {2.0 -1.0}
finish_test
