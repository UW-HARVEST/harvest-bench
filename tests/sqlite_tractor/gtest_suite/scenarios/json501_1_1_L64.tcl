source $::suite_root/glue/tester.tcl
do_execsql_test 1.1 {
  WITH c(x) AS (VALUES('{a:5,b:6}'))
  SELECT x->>'a', json(x), json_valid(x), NOT json_error_position(x) FROM c;
} {5 {{"a":5,"b":6}} 0 1}
finish_test
