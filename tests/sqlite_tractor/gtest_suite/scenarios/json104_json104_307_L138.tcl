source $::suite_root/glue/tester.tcl
do_execsql_test json104-307 {
  SELECT json_patch('{"a":[{"b":"c"}]}','{"a":[1]}');
} {{{"a":[1]}}}
finish_test
