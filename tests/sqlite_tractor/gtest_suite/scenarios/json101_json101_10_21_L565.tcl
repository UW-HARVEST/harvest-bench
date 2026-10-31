source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.21 {
  SELECT json_valid('" \4 "');
} {0}
finish_test
