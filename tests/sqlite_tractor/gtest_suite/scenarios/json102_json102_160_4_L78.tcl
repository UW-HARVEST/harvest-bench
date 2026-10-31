source $::suite_root/glue/tester.tcl
do_execsql_test json102-160-4 {
  SELECT json(jsonb_array(jsonb_array(1,2)));
} {{[[1,2]]}}
finish_test
