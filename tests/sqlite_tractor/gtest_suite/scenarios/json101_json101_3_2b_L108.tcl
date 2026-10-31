source $::suite_root/glue/tester.tcl
do_execsql_test json101-3.2b {
  SELECT json_replace('{"a":1,"b":2}','$.a',jsonb('[3,4,5]'));
} {{{"a":[3,4,5],"b":2}}}
finish_test
