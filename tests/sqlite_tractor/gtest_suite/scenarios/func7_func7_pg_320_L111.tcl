source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-320 {
   SELECT round( atan(1), 7);
} {0.7853982}
finish_test
