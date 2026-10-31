source $::suite_root/glue/tester.tcl
do_execsql_test json102-445-7 {
  SELECT json_remove('[0,1,2,3,4]','$[42949672960]');
} {{[0,1,2,3,4]}}
finish_test
