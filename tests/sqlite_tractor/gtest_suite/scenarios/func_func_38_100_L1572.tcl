source $::suite_root/glue/tester.tcl
do_execsql_test func-38.100 {
  WITH t1(x) AS (VALUES(9e+999)) SELECT sum(x), avg(x), total(x) FROM t1;
  WITH t1(x) AS (VALUES(-9e+999)) SELECT sum(x), avg(x), total(x) FROM t1;
} {Inf Inf Inf -Inf -Inf -Inf}
finish_test
