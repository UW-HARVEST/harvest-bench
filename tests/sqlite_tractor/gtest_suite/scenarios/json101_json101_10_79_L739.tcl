source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.79 {
  SELECT json_valid('" \n "');
} {1}
finish_test
