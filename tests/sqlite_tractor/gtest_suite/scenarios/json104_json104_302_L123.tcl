source $::suite_root/glue/tester.tcl
do_execsql_test json104-302 {
  SELECT json_patch('{"a":"b"}','{"a":null}');
} {{{}}}
finish_test
