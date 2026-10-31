source $::suite_root/glue/tester.tcl
do_execsql_test func7-mysql-260 {
   SELECT mod(234,10), 253%7, mod(29,9), 29%9;
} {4.0 1 2.0 2}
finish_test
