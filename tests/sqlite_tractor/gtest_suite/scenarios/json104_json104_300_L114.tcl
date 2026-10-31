source $::suite_root/glue/tester.tcl
do_execsql_test json104-300 {
  SELECT json_patch('{"a":"b"}','{"a":"c"}');
} {{{"a":"c"}}}
finish_test
