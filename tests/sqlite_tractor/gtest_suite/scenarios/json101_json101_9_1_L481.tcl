source $::suite_root/glue/tester.tcl
do_execsql_test json101-9.1 {
  SELECT json_quote('abc"xyz');
} {{"abc\"xyz"}}
finish_test
