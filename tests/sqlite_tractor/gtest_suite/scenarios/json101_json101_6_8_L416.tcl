source $::suite_root/glue/tester.tcl
do_execsql_test json101-6.8 {
  SELECT json_error_position('["a",55,"b",72,]');
} {0}
finish_test
