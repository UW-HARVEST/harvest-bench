source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-230 {
   SELECT log(2,65536), log(10,100), quote(log(1,100)), quote(log(0,100));
} {16.0 2.0 NULL NULL}
finish_test
