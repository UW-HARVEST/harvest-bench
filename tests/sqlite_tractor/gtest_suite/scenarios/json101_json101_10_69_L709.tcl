source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.69 {
  SELECT json_valid('" \d "');
} {0}
finish_test
