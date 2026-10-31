source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.39 {
  SELECT json_valid('" \F "');
} {0}
finish_test
