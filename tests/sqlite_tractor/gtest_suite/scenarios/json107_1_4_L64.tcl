source $::suite_root/glue/tester.tcl
do_execsql_test 1.4 {
  SELECT json_remove(CAST('{"a":123,"b":456}' AS blob),'$.a');
} {{{"b":456}}}
finish_test
