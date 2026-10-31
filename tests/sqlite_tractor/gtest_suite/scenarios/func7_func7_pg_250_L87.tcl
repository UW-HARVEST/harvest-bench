source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-250 {
   SELECT round(42.4382,2);
} {42.44}
finish_test
