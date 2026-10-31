source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-240 {
   SELECT log2(65536), quote(log2(-100)), quote(log2(0));
} {16.0 NULL NULL}
finish_test
