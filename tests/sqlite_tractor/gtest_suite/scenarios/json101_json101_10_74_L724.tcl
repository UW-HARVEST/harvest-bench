source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.74 {
  SELECT json_valid('" \i "');
} {0}
finish_test
