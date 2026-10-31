source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.34 {
  SELECT json_valid('" \A "');
} {0}
finish_test
