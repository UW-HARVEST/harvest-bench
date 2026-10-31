source $::suite_root/glue/tester.tcl
do_execsql_test 1.5 {
  SELECT json_array_insert('[1,2,3]','$[3]',888);
} {{[1,2,3,888]}}
finish_test
