source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-220 {
   SELECT power(9,3);
} {729.0}
finish_test
