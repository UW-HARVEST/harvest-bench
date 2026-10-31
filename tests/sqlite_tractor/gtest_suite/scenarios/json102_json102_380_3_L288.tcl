source $::suite_root/glue/tester.tcl
do_execsql_test json102-380-3 {
  SELECT json(jsonb_set('{"a":2,"c":4}', '$.c', '[97,96]'));
} {{{"a":2,"c":"[97,96]"}}}
finish_test
