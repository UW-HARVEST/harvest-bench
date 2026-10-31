source $::suite_root/glue/tester.tcl
do_execsql_test json102-500-2 {
  SELECT json_remove(jsonb('{"x":25,"y":42}'),'$');
} {{}}
finish_test
