source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-210 {
   SELECT round(ln(2),7), quote(ln(-2));
} {0.6931472 NULL}
finish_test
