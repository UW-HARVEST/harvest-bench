source $::suite_root/glue/tester.tcl
do_execsql_test json101-6.6 {
  SELECT json_error_position('{"a":55,"b":72,,}');
} {16}
finish_test
