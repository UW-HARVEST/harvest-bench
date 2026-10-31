source $::suite_root/glue/tester.tcl
do_execsql_test json101-3.4b {
  SELECT json_type(jsonb_set('{"a":1,"b":2}','$.b',jsonb('{"x":3,"y":4}')),'$.b');
} {object}
finish_test
