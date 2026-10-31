source $::suite_root/glue/tester.tcl
do_execsql_test json101-9.2 {
  SELECT json_quote(3.14159);
} {3.14159}
finish_test
