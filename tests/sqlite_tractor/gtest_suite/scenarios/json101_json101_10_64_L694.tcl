source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.64 {
  SELECT json_valid('" \_ "');
} {0}
finish_test
