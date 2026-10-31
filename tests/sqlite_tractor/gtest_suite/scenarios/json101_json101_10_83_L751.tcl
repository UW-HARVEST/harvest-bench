source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.83 {
  SELECT json_valid('" \r "');
} {1}
finish_test
