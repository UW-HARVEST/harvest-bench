source $::suite_root/glue/tester.tcl
do_execsql_test 8.9 {
  WITH c(x) AS (VALUES('{x: .5e3}')) SELECT x->>'x', json(x) FROM c;
} {500.0 {{"x":0.5e3}}}
finish_test
