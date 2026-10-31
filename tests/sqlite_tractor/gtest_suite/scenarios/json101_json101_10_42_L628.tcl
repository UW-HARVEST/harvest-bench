source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.42 {
  SELECT json_valid('" \I "');
} {0}
finish_test
