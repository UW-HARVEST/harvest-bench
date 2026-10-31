source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.57 {
  SELECT json_valid('" \X "');
} {0}
finish_test
