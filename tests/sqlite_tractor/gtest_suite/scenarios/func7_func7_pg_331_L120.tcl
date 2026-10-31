source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-331 {
   SELECT degrees( atan2(1,0) );
} {90.0}
finish_test
