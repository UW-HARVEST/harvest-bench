source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.19 {
  SELECT json_valid('" \2 "');
} {0}
finish_test
