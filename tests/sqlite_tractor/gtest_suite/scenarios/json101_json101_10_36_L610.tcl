source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.36 {
  SELECT json_valid('" \C "');
} {0}
finish_test
