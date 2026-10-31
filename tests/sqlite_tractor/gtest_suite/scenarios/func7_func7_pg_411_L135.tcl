source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-411 {
   SELECT sin( radians(30) );
} {0.5}
finish_test
