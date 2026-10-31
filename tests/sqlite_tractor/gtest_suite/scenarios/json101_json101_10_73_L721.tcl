source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.73 {
  SELECT json_valid('" \h "');
} {0}
finish_test
