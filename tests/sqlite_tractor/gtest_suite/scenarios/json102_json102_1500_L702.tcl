source $::suite_root/glue/tester.tcl
do_execsql_test json102-1500 {
  WITH RECURSIVE c(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM c WHERE x<0x20)
  SELECT x FROM c WHERE json_valid(printf('{"a":"x%sz"}', char(x))) ORDER BY x;
} {32}
finish_test
