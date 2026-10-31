source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.81 {
  SELECT json_valid('" \p "');
} {0}
finish_test
