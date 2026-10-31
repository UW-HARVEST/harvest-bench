source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-401 {
   SELECT cos( radians(60.0) );
} {0.5}
finish_test
