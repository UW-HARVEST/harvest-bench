source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-320 {
   SELECT sqrt(4), round(sqrt(20),7), quote(sqrt(-16));
} {2.0 4.472136 NULL}
finish_test
