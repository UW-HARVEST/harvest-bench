source $::suite_root/glue/tester.tcl
do_execsql_test json102-450-4 {
  SELECT json(jsonb_remove(jsonb('[0,1,2,3,4]'),'$[2]','$[0]'));
} {{[1,3,4]}}
finish_test
