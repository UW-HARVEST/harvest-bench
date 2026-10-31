source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-260 {
   SELECT sign(-8.4);
} {-1}
finish_test
