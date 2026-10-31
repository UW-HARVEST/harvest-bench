source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.22 {
  SELECT json_valid('" \5 "');
} {0}
finish_test
