source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-510 {
   SELECT round( cosh(0), 7);
} {1.0}
finish_test
