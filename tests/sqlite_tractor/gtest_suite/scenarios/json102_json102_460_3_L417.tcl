source $::suite_root/glue/tester.tcl
do_execsql_test json102-460-3 {
  SELECT json(jsonb_remove('[0,1,2,3,4]','$[0]','$[2]'));
} {{[1,2,4]}}
finish_test
