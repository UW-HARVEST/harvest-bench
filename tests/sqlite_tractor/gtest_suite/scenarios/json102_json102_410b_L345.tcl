source $::suite_root/glue/tester.tcl
do_execsql_test json102-410b {
  SELECT json(jsonb_object('a',2,'c',4));
} {{{"a":2,"c":4}}}
finish_test
