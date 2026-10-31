source $::suite_root/glue/tester.tcl
do_execsql_test json104-308 {
  SELECT json_patch('["a","b"]','["c","d"]');
} {{["c","d"]}}
finish_test
