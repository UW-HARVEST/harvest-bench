source $::suite_root/glue/tester.tcl
do_execsql_test json102-190b {
  SELECT json_array_length(jsonb('[1,2,3,4]'));
} {{4}}
finish_test
