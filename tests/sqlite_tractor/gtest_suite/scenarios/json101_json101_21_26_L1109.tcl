source $::suite_root/glue/tester.tcl
do_execsql_test json101-21.26 {
  WITH c(x) AS (VALUES(1),(2.0),(NULL),('three'))
  SELECT json_group_array(x) FROM c;
} {[1,2.0,null,"three"]}
finish_test
