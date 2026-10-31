source $::suite_root/glue/tester.tcl
do_execsql_test json104-222 {
  SELECT json_patch('{}','{"a":{"bb":{"ccc":[1,{"dddd":null},3]}}}');
} {{{"a":{"bb":{"ccc":[1,{"dddd":null},3]}}}}}
finish_test
