source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.85 {
  SELECT json_valid('" \t "');
} {1}
finish_test
