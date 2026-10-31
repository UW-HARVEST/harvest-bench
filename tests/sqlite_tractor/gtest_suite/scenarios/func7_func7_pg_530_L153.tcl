source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-530 {
   SELECT round( asinh(1), 7);
} {0.8813736}
finish_test
