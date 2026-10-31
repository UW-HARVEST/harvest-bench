source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.86.4 {
  SELECT json_valid('" \uabcd "');
} {1}
finish_test
