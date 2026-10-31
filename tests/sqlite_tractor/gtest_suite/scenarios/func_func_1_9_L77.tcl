source $::suite_root/glue/tester.tcl
do_execsql_test func-1.9 {
  WITH c(x) AS (VALUES(char(350,351,352,353,354)))
  SELECT length(x), octet_length(x) FROM c;
} {5 10}
finish_test
