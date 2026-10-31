source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.55 {
  SELECT json_valid('" \V "');
} {0}
finish_test
