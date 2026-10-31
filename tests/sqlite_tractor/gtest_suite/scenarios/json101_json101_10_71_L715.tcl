source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.71 {
  SELECT json_valid('" \f "');
} {1}
finish_test
