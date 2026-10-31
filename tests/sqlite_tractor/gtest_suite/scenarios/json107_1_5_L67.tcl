source $::suite_root/glue/tester.tcl
do_execsql_test 1.5 {
  SELECT json_set(CAST('{"a":123,"b":456}' AS blob),'$.a',789);
} {{{"a":789,"b":456}}}
finish_test
