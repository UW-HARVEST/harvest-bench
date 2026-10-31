source $::suite_root/glue/tester.tcl
do_execsql_test 8.10 {
  WITH c(x) AS (VALUES('{x: -.5e-1}')) SELECT x->>'x', json(x) FROM c;
} {-0.05 {{"x":-0.5e-1}}}
finish_test
