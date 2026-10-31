source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-280 {
   SELECT trunc(42.8), trunc(-42.8);
} {42.0 -42.0}
finish_test
