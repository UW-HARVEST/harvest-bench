source $::suite_root/glue/tester.tcl
do_execsql_test 8.11 {
  WITH c(x) AS (VALUES('{x: +.5e-2}')) SELECT x->>'x', json(x) FROM c;
} {0.005 {{"x":0.5e-2}}}
finish_test
