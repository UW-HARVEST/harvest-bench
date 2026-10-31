source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.89 {
  SELECT json_valid('" \x "');
} {0}
finish_test
