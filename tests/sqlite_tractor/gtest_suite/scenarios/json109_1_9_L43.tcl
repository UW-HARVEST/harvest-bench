source $::suite_root/glue/tester.tcl
do_execsql_test 1.9 {
  SELECT json_array_insert('[1,2,3]','$[#-4]',888);
} {{[1,2,3]}}
finish_test
