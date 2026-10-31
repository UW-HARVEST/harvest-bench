source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-330 {
   SELECT round( atan2(1,0), 7);
} {1.5707963}
finish_test
