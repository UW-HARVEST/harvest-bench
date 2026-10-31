source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-190 {
   SELECT round( exp(2), 7), round( exp(-2), 7), exp(0);
} {7.3890561 0.1353353 1.0}
finish_test
