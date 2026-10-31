source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-310 {
   SELECT round( asin(1), 7);
} {1.5707963}
finish_test
