source $::suite_root/glue/tester.tcl
do_execsql_test json101-3.1 {
  SELECT json_replace('{"a":1,"b":2}','$.a','[3,4,5]');
} {{{"a":"[3,4,5]","b":2}}}
finish_test
