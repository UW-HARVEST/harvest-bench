source $::suite_root/glue/tester.tcl
do_execsql_test json101-21.20 {
  SELECT json_set('{a:5,b:7}',NULL,NULL);
} {{{"a":5,"b":7}}}
finish_test
