source $::suite_root/glue/tester.tcl
do_execsql_test json102-350-2 {
  SELECT json_replace(jsonb('{"a":2,"c":4}'), '$.e', 99);
} {{{"a":2,"c":4}}}
finish_test
