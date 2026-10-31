source $::suite_root/glue/tester.tcl
do_execsql_test json101-16.30 {
  SELECT unicode(json_extract('"\uD834\uDD1E"','$'));
} {119070}
finish_test
