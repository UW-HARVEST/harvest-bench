source $::suite_root/glue/tester.tcl
do_execsql_test json102-1831 {
  SELECT '["zero","one","two"]'->1;
} {{"one"}}
finish_test
