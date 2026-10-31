source $::suite_root/glue/tester.tcl
do_execsql_test json102-290-2 {
  SELECT json_extract(jsonb('{"a":2,"c":[4,5],"f":7}'),'$.c','$.a');
} {{[[4,5],2]}}
finish_test
