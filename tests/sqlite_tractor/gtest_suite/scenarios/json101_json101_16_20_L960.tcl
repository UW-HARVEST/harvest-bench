source $::suite_root/glue/tester.tcl
do_execsql_test json101-16.20 {
  SELECT length(json_extract('"\uD834\uDD1E"','$'));
} {1}
finish_test
