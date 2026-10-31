source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.87 {
  SELECT json_valid('" \v "');
} {0}
finish_test
