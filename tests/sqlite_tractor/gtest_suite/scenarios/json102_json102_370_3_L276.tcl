source $::suite_root/glue/tester.tcl
do_execsql_test json102-370-3 {
  SELECT json(jsonb_set('{"a":2,"c":4}', '$.e', 99));
} {{{"a":2,"c":4,"e":99}}}
finish_test
