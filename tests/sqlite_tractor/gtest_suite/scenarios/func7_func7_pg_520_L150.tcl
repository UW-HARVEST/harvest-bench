source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-520 {
   SELECT round( tanh(1), 7);
} {0.7615942}
finish_test
