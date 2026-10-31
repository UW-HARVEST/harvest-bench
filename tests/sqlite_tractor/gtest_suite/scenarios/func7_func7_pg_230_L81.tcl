source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-230 {
   SELECT round(radians(45.0),7);
} {0.7853982}
finish_test
