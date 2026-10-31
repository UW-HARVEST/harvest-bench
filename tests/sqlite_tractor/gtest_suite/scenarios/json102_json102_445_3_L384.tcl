source $::suite_root/glue/tester.tcl
do_execsql_test json102-445-3 {
  SELECT json_remove('[0,1,2,3,4]','$[4294967295]');
} {{[0,1,2,3,4]}}
finish_test
