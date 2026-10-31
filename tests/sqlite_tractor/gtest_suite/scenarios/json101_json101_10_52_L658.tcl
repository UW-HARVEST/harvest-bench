source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.52 {
  SELECT json_valid('" \S "');
} {0}
finish_test
