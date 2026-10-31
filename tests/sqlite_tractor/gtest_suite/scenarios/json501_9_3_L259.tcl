source $::suite_root/glue/tester.tcl
do_execsql_test 9.3 {
  WITH c(x) AS (VALUES('{x: Infinity}')) SELECT x->>'x', json(x) FROM c;
} {Inf {{"x":9e999}}}
finish_test
