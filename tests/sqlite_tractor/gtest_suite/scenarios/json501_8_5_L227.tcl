source $::suite_root/glue/tester.tcl
do_execsql_test 8.5 {
  WITH c(x) AS (VALUES('{x: +.5}')) SELECT x->>'x', json(x) FROM c;
} {0.5 {{"x":0.5}}}
finish_test
