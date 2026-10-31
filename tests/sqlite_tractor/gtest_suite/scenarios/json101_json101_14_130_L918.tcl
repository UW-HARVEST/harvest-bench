source $::suite_root/glue/tester.tcl
do_execsql_test json101-14.130 {
  SELECT fullkey FROM json_each('null');
} {$}
finish_test
