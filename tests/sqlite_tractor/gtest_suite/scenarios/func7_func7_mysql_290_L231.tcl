source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-290 {
   SELECT round(radians(90),7);
} {1.5707963}
finish_test
