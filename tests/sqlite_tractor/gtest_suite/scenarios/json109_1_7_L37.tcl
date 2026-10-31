source $::suite_root/glue/tester.tcl
do_execsql_test 1.7 {
  SELECT json_array_insert('[1,2,3]','$[#-2]',888);
} {{[1,888,2,3]}}
finish_test
