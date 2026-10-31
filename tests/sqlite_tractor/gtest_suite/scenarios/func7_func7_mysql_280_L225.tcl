source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-280 {
   SELECT pow(2,2), pow(2,-2);
} {4.0 0.25}
finish_test
