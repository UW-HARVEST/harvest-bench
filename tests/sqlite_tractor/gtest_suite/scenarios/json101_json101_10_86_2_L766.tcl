source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.86.2 {
  SELECT json_valid('" \uab "');
} {0}
finish_test
