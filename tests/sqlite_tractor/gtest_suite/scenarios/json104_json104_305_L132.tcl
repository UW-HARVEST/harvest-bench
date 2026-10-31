source $::suite_root/glue/tester.tcl
do_execsql_test json104-305 {
  SELECT json_patch('{"a":"c"}','{"a":["b"]}');
} {{{"a":["b"]}}}
finish_test
