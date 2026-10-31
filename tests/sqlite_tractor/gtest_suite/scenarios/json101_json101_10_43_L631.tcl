source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.43 {
  SELECT json_valid('" \J "');
} {0}
finish_test
