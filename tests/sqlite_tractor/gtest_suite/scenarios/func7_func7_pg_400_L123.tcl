source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-400 {
   SELECT cos(0);
} {1.0}
finish_test
