source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-410 {
   SELECT round( sin(1), 7);
} {0.841471}
finish_test
