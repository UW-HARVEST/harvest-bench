source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-190 {
   SELECT degrees(pi()), degrees(pi()/2);
} {180.0 90.0}
finish_test
