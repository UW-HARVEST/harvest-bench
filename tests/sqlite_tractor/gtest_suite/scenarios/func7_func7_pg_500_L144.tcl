source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-500 {
   SELECT round( sinh(1), 7);
} {1.1752012}
finish_test
