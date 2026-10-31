source $::suite_root/glue/tester.tcl
do_execsql_test json104-310 {
  SELECT json_patch('{"a":"foo"}','null');
} {{null}}
finish_test
