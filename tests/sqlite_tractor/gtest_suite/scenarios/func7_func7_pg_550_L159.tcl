source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-550 {
   SELECT round( atanh(0.5), 7);
} {0.5493061}
finish_test
