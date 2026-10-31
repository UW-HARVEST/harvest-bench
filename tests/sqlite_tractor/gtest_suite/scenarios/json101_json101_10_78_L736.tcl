source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.78 {
  SELECT json_valid('" \m "');
} {0}
finish_test
