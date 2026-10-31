source $::suite_root/glue/tester.tcl
do_execsql_test json101-16.10 {
  SELECT length(json_extract('"abc\uD834\uDD1Exyz"','$'));
} {7}
finish_test
