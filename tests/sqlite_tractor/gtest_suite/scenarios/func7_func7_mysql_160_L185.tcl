source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-160 {
   SELECT round( atan2(-2,2), 7), round( atan2(pi(),0), 7);
} {-0.7853982 1.5707963}
finish_test
