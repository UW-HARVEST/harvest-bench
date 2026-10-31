source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.16 {
  SELECT json_valid('" \/ "');
} {1}
finish_test
