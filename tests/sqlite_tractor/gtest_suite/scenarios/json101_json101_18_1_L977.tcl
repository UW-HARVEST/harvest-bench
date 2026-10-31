source $::suite_root/glue/tester.tcl
do_execsql_test json101-18.1 {
  SELECT json_valid('{"":5}');
} {1}
finish_test
