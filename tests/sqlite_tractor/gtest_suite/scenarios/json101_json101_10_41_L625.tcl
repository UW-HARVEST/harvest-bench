source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.41 {
  SELECT json_valid('" \H "');
} {0}
finish_test
