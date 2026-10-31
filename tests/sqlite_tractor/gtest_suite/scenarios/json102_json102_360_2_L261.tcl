source $::suite_root/glue/tester.tcl
do_execsql_test json102-360-2 {
  SELECT json_set(jsonb('{"a":2,"c":4}'), '$.a', 99);
} {{{"a":99,"c":4}}}
finish_test
