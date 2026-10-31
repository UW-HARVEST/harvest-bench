source $::suite_root/glue/tester.tcl
do_execsql_test json102-140 {
  SELECT json_array(1,2,'3',4);
} {{[1,2,"3",4]}}
finish_test
