source $::suite_root/glue/tester.tcl
do_execsql_test 1.1 {
  SELECT json_array_insert('[1,2,3]','$[0]',999,'$[0]',888);
} {{[888,999,1,2,3]}}
finish_test
