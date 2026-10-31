source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.46 {
  SELECT json_valid('" \M "');
} {0}
finish_test
