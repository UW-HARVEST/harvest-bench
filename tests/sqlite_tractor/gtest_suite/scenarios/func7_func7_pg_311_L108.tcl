source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-311 {
   SELECT format('%f',degrees( asin(0.5) ));
} {30.0}
finish_test
