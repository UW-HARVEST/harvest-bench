source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-331 {
   SELECT round(tan(pi()+1),7);
} {1.5574077}
finish_test
