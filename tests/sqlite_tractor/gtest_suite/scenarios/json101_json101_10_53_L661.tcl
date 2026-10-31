source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.53 {
  SELECT json_valid('" \T "');
} {0}
finish_test
