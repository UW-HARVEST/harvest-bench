source $::suite_root/glue/tester.tcl
do_execsql_test json102-350-3 {
  SELECT json(jsonb_replace('{"a":2,"c":4}', '$.e', 99));
} {{{"a":2,"c":4}}}
finish_test
