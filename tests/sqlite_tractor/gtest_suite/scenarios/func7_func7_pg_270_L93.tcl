source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-270 {
   SELECT round( sqrt(2), 7);
} {1.4142136}
finish_test
