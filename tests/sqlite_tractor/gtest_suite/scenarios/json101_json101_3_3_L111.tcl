source $::suite_root/glue/tester.tcl
do_execsql_test json101-3.3 {
  SELECT json_type(json_set('{"a":1,"b":2}','$.b','{"x":3,"y":4}'),'$.b');
} {text}
finish_test
