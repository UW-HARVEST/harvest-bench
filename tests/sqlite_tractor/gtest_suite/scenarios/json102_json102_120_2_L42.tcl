source $::suite_root/glue/tester.tcl
do_execsql_test json102-120-2 {
  SELECT json(jsonb_object('ex',json_array(52,3.14159)));
} {{{"ex":[52,3.14159]}}}
finish_test
