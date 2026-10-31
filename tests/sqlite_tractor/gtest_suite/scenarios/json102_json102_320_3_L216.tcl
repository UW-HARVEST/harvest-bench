source $::suite_root/glue/tester.tcl
do_execsql_test json102-320-3 {
  SELECT json(jsonb_insert('{"a":2,"c":4}', '$.a', 99));
} {{{"a":2,"c":4}}}
finish_test
