source $::suite_root/glue/tester.tcl
do_execsql_test json104-311 {
  SELECT json_patch('{"a":"foo"}','"bar"');
} {{"bar"}}
finish_test
