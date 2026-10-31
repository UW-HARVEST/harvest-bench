source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.44 {
  SELECT json_valid('" \K "');
} {0}
finish_test
