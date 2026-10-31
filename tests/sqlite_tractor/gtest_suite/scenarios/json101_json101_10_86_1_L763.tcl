source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.86.1 {
  SELECT json_valid('" \ua "');
} {0}
finish_test
