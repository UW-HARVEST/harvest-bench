source $::suite_root/glue/tester.tcl
do_execsql_test json102-390-5 {
  SELECT json_set('{"a":2,"c":4}', '$.c', jsonb('[97,96]'));
} {{{"a":2,"c":[97,96]}}}
finish_test
