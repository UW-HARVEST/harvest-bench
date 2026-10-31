source $::suite_root/glue/tester.tcl
do_execsql_test 9.4 {
  WITH c(x) AS (VALUES('{x: NaN}')) SELECT x->>'x', json(x) FROM c;
} {{} {{"x":null}}}
finish_test
