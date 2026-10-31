source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-210 {
   SELECT round(pi(),7);
} {3.1415927}
finish_test
