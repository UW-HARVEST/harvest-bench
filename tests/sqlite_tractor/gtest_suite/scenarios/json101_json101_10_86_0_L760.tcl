source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.86.0 {
  SELECT json_valid('" \u "');
} {0}
finish_test
