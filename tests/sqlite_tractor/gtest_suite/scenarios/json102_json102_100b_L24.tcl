source $::suite_root/glue/tester.tcl
do_execsql_test json102-100b {
  SELECT json(jsonb_object('ex','[52,3.14159]'));
} {{{"ex":"[52,3.14159]"}}}
finish_test
