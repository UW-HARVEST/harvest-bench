source $::suite_root/glue/tester.tcl
do_execsql_test 1.6 {
  SELECT json_array_insert('[1,2,3]','$[#-1]',888);
} {{[1,2,888,3]}}
finish_test
