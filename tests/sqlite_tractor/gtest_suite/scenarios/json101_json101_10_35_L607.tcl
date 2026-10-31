source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.35 {
  SELECT json_valid('" \B "');
} {0}
finish_test
