source $::suite_root/glue/tester.tcl
do_execsql_test 8.7 {
  WITH c(x) AS (VALUES('{x: +4.e1}')) SELECT x->>'x', json(x) FROM c;
} {40.0 {{"x":4.0e1}}}
finish_test
