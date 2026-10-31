source $::suite_root/glue/tester.tcl
do_execsql_test 8.8 {
  WITH c(x) AS (VALUES('{x: -4.e2}')) SELECT x->>'x', json(x) FROM c;
} {-400.0 {{"x":-4.0e2}}}
finish_test
