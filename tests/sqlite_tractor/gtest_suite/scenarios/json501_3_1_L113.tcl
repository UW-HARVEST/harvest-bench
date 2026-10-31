source $::suite_root/glue/tester.tcl
do_execsql_test 3.1 {
  WITH c(x) AS (VALUES('[5, 6,]'))
  SELECT x->>1, json(x), json_valid(x), NOT json_error_position(x) FROM c;
} {6 {[5,6]} 0 1}
finish_test
