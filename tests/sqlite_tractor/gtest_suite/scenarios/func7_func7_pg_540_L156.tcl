source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-540 {
   SELECT round( acosh(1), 7);
} {0.0}
finish_test
