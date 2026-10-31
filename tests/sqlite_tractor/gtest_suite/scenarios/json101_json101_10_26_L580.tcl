source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.26 {
  SELECT json_valid('" \9 "');
} {0}
finish_test
