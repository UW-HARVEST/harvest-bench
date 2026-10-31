source $::suite_root/glue/tester.tcl
do_execsql_test json102-230b {
  SELECT json_array_length(jsonb('{"one":[1,2,3]}'), '$.one');
} {{3}}
finish_test
