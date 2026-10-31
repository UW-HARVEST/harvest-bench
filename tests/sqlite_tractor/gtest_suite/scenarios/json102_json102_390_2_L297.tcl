source $::suite_root/glue/tester.tcl
do_execsql_test json102-390-2 {
  SELECT json_set(jsonb('{"a":2,"c":4}'), '$.c', json('[97,96]'));
} {{{"a":2,"c":[97,96]}}}
finish_test
