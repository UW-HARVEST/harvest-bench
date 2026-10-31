source $::suite_root/glue/tester.tcl
do_execsql_test json104-306 {
  SELECT json_patch('{"a":{"b":"c"}}','{"a":{"b":"d","c":null}}');
} {{{"a":{"b":"d"}}}}
finish_test
