source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.13 {
  SELECT json_valid('" \, "');
} {0}
finish_test
