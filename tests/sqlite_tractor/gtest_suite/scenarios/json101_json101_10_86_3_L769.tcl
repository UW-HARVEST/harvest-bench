source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.86.3 {
  SELECT json_valid('" \uabc "');
} {0}
finish_test
