source $::suite_root/glue/tester.tcl
do_execsql_test json101-9.4 {
  SELECT json_quote(null);
} {"null"}
finish_test
