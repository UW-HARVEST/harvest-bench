source $::suite_root/glue/tester.tcl
do_execsql_test json101-21.27 {
  WITH c(x,y) AS (VALUES('a',1),('b',2.0),('c',NULL),(NULL,'three'),('e','four'))
  SELECT json_group_object(x,y) FROM c;
} {{{"a":1,"b":2.0,"c":null,"e":"four"}}}
finish_test
