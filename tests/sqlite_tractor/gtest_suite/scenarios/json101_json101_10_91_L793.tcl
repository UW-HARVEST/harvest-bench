source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.91 {
  SELECT json_valid('" \z "');
} {0}
finish_test
