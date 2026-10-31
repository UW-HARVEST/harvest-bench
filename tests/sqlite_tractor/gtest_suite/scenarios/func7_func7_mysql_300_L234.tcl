source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-300 {
   SELECT sign(-32), sign(0), sign(234);
} {-1 0 1}
finish_test
