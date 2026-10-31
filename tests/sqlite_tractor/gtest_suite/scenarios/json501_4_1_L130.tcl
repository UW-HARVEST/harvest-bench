source $::suite_root/glue/tester.tcl
do_execsql_test 4.1 {
  WITH c(x) AS (VALUES('{"a": ''abcd''}'))
  SELECT x->>'a', json(x), json_valid(x), NOT json_error_position(x) FROM c;
} {abcd {{"a":"abcd"}} 0 1}
finish_test
