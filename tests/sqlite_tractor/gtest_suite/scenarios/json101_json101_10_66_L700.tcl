source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.66 {
  SELECT json_valid('" \a "');
} {0}
finish_test
