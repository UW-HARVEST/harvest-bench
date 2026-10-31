source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.77 {
  SELECT json_valid('" \l "');
} {0}
finish_test
