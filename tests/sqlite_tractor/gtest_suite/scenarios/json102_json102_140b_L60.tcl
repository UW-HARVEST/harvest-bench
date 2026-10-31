source $::suite_root/glue/tester.tcl
do_execsql_test json102-140b {
  SELECT json(jsonb_array(1,2,'3',4));
} {{[1,2,"3",4]}}
finish_test
