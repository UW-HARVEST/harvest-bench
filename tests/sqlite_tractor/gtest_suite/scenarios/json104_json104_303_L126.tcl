source $::suite_root/glue/tester.tcl
do_execsql_test json104-303 {
  SELECT json_patch('{"a":"b","b":"c"}','{"a":null}');
} {{{"b":"c"}}}
finish_test
