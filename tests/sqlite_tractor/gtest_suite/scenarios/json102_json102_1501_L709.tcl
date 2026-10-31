source $::suite_root/glue/tester.tcl
do_execsql_test json102-1501 {
  WITH RECURSIVE c(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM c WHERE x<0x1f)
  SELECT sum(json_valid(json_quote('a'||char(x)||'z'))) FROM c ORDER BY x;
} {31}
finish_test
