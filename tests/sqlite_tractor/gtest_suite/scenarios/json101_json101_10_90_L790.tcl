source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.90 {
  SELECT json_valid('" \y "');
} {0}
finish_test
