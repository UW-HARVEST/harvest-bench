source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.48 {
  SELECT json_valid('" \O "');
} {0}
finish_test
