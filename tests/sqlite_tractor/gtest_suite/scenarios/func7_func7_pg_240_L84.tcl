source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-240 {
   SELECT round(42.4);
} {42.0}
finish_test
