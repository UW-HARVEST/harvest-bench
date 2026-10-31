source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.86.5 {
  SELECT json_valid('" \uFEDC "');
} {1}
finish_test
