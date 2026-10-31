source $::suite_root/glue/tester.tcl
do_execsql_test json102-190 {
  SELECT json_array_length('[1,2,3,4]');
} {{4}}
finish_test
