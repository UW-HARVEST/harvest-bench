source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.50 {
  SELECT json_valid('" \Q "');
} {0}
finish_test
