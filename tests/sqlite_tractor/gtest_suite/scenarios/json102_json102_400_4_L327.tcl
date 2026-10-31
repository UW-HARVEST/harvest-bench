source $::suite_root/glue/tester.tcl
do_execsql_test json102-400-4 {
  SELECT json(jsonb_set(jsonb('{"a":2,"c":4}'), '$.c', json_array(97,96)));
} {{{"a":2,"c":[97,96]}}}
finish_test
