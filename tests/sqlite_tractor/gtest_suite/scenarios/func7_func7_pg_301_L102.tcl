source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-301 {
   SELECT format('%f',degrees(acos(0.5)));
} {60.0}
finish_test
