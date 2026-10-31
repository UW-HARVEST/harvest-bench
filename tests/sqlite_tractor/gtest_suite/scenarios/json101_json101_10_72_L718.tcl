source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.72 {
  SELECT json_valid('" \g "');
} {0}
finish_test
