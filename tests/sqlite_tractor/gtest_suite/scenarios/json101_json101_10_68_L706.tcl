source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.68 {
  SELECT json_valid('" \c "');
} {0}
finish_test
