source $::suite_root/glue/tester.tcl
do_execsql_test 1.3 {
  SELECT json_insert(CAST('{"a":123}' AS blob),'$.b',456);
} {{{"a":123,"b":456}}}
finish_test
