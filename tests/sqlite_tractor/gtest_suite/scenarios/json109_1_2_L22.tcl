source $::suite_root/glue/tester.tcl
do_execsql_test 1.2 {
  SELECT json_array_insert('[1,2,3]','$[0]',999,'$[#]',888);
} {{[999,1,2,3,888]}}
finish_test
