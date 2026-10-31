source $::suite_root/glue/tester.tcl
do_execsql_test json101-21.4 {
  SELECT json_array(NULL);
} {[null]}
finish_test
