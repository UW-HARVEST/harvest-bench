source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.76 {
  SELECT json_valid('" \k "');
} {0}
finish_test
