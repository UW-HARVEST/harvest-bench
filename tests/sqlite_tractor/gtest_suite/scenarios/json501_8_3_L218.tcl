source $::suite_root/glue/tester.tcl
do_execsql_test 8.3 {
  WITH c(x) AS (VALUES('{x: -4.}')) SELECT x->>'x', json(x) FROM c;
} {-4.0 {{"x":-4.0}}}
finish_test
