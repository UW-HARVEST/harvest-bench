source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.49 {
  SELECT json_valid('" \P "');
} {0}
finish_test
