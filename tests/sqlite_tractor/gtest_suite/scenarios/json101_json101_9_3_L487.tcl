source $::suite_root/glue/tester.tcl
do_execsql_test json101-9.3 {
  SELECT json_quote(12345);
} {12345}
finish_test
