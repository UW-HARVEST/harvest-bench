source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.82 {
  SELECT json_valid('" \q "');
} {0}
finish_test
