source $::suite_root/glue/tester.tcl
do_execsql_test json102-160 {
  SELECT json_array(json_array(1,2));
} {{[[1,2]]}}
finish_test
