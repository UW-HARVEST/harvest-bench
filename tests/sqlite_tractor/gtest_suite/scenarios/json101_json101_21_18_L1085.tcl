source $::suite_root/glue/tester.tcl
do_execsql_test json101-21.18 {
  SELECT json_replace('{a:5,b:7}',NULL,NULL);
} {{{"a":5,"b":7}}}
finish_test
