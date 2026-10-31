source $::suite_root/glue/tester.tcl
do_execsql_test json104-310a {
  SELECT coalesce(json_patch('{"a":"foo"}',null), 'real-null');
} {{real-null}}
finish_test
