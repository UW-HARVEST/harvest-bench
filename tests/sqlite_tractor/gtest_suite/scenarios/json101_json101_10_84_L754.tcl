source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.84 {
  SELECT json_valid('" \s "');
} {0}
finish_test
