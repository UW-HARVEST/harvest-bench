source $::suite_root/glue/tester.tcl
do_execsql_test json104-301 {
  SELECT json_patch('{"a":"b"}','{"b":"c"}');
} {{{"a":"b","b":"c"}}}
finish_test
