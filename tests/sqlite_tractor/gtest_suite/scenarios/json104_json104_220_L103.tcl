source $::suite_root/glue/tester.tcl
do_execsql_test json104-220 {
  SELECT json_patch('{}','{"a":{"bb":{"ccc":null}}}');
} {{{"a":{"bb":{}}}}}
finish_test
