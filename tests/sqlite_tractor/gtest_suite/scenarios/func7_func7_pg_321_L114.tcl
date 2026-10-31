source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-321 {
   SELECT degrees( atan(1) );
} {45.0}
finish_test
