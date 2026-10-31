source $::suite_root/glue/tester.tcl
do_execsql_test json101-3.4 {
  SELECT json_type(json_set('{"a":1,"b":2}','$.b',json('{"x":3,"y":4}')),'$.b');
} {object}
finish_test
