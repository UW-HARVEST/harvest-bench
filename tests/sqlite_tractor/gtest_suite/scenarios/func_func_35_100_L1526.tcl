source $::suite_root/glue/tester.tcl
do_execsql_test func-35.100 {
  CREATE TABLE t1(x);
  SELECT coalesce(x, abs(-9223372036854775808)) FROM t1;
} {}
finish_test
