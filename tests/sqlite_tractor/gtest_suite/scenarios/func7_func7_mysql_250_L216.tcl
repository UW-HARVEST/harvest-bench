source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-250 {
   SELECT round(log10(2),7), log10(100), quote(log10(-100));
} {0.30103 2.0 NULL}
finish_test
