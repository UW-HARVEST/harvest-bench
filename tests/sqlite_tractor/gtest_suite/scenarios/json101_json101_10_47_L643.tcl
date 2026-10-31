source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.47 {
  SELECT json_valid('" \N "');
} {0}
finish_test
