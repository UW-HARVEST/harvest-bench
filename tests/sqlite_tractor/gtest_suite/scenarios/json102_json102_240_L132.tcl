source $::suite_root/glue/tester.tcl
do_execsql_test json102-240 {
  SELECT json_array_length('{"one":[1,2,3]}', '$.two');
} {{}}
finish_test
