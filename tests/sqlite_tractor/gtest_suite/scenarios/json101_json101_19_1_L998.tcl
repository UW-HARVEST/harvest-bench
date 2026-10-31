source $::suite_root/glue/tester.tcl
do_execsql_test json101-19.1 {
  DROP TABLE IF EXISTS t1;
  CREATE TABLE t1(x);
} {}
finish_test
