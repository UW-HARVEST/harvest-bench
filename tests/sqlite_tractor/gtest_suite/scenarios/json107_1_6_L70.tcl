source $::suite_root/glue/tester.tcl
do_execsql_test 1.6 {
  SELECT json_replace(CAST('{"a":123,"b":456}' AS blob),'$.a',789);
} {{{"a":789,"b":456}}}
finish_test
