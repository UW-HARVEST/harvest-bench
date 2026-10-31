source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-200 {
   SELECT mod(9,4);
} {1.0}
finish_test
