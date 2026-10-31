source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.40 {
  SELECT json_valid('" \G "');
} {0}
finish_test
