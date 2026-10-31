source $::suite_root/glue/tester.tcl
do_execsql_test json102-480-3 {
  SELECT json(jsonb_remove('{"x":25,"y":42}','$.z'));
} {{{"x":25,"y":42}}}
finish_test
