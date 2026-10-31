source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.45 {
  SELECT json_valid('" \L "');
} {0}
finish_test
