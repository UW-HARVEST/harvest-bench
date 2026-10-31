source $::suite_root/glue/tester.tcl
do_execsql_test 1.8 {
  SELECT json(CAST('{"a":123,"b":456}' AS blob));
} {{{"a":123,"b":456}}}
finish_test
