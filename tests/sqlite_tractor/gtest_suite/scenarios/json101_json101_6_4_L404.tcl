source $::suite_root/glue/tester.tcl
do_execsql_test json101-6.4 {
  SELECT json_valid('{"a":55,"b":72 , }');
} {0}
finish_test
