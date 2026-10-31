source $::suite_root/glue/tester.tcl
do_execsql_test json102-150b {
  SELECT json(jsonb_array('[1,2]'));
} {{["[1,2]"]}}
finish_test
