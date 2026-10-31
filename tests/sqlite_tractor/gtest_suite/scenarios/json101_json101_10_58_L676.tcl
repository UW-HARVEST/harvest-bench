source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.58 {
  SELECT json_valid('" \Y "');
} {0}
finish_test
