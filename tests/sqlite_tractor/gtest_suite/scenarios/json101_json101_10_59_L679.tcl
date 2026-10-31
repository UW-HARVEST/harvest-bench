source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.59 {
  SELECT json_valid('" \Z "');
} {0}
finish_test
