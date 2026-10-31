source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.80 {
  SELECT json_valid('" \o "');
} {0}
finish_test
