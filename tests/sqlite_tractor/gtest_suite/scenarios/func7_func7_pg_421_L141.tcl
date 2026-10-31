source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-421 {
   SELECT round(tan( radians(45) ),10);
} {1.0}
finish_test
