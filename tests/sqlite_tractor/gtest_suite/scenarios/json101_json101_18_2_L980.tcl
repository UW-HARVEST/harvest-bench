source $::suite_root/glue/tester.tcl
do_execsql_test json101-18.2 {
  SELECT json_extract('{"":5}', '$.""');
} {5}
finish_test
