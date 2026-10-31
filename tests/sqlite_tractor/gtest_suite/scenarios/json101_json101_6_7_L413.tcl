source $::suite_root/glue/tester.tcl
do_execsql_test json101-6.7 {
  SELECT json_valid('{"a":55,"b":72}');
} {1}
finish_test
