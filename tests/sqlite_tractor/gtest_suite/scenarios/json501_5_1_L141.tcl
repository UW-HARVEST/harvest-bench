source $::suite_root/glue/tester.tcl
do_execsql_test 5.1 {
  WITH c(x) AS (VALUES('{a: "abc'||char(0x5c,0x0a)||'xyz"}'))
  SELECT x->>'a', json(x), json_valid(x), NOT json_error_position(x) FROM c;
} {abcxyz {{"a":"abcxyz"}} 0 1}
finish_test
