source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-330 {
   SELECT tan(pi()) BETWEEN -1.0e-15 AND 1.0e-15;
} {1}
finish_test
