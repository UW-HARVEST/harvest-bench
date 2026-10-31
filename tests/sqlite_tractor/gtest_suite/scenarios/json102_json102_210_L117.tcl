source $::suite_root/glue/tester.tcl
do_execsql_test json102-210 {
  SELECT json_array_length('[1,2,3,4]', '$[2]');
} {{0}}
finish_test
