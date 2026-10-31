source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-420 {
   SELECT round( tan(1), 7);
} {1.5574077}
finish_test
