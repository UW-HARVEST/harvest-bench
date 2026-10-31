source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.18 {
  SELECT json_valid('" \1 "');
} {0}
finish_test
