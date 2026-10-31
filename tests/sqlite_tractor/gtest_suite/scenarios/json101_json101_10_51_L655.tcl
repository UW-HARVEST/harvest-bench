source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.51 {
  SELECT json_valid('" \R "');
} {0}
finish_test
