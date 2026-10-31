source $::suite_root/glue/tester.tcl
do_execsql_test json104-309 {
  SELECT json_patch('{"a":"b"}','["c"]');
} {{["c"]}}
finish_test
