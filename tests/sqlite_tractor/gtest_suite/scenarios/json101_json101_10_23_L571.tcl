source $::suite_root/glue/tester.tcl
do_execsql_test json101-10.23 {
  SELECT json_valid('" \6 "');
} {0}
finish_test
