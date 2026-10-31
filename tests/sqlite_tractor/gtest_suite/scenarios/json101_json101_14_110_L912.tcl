source $::suite_root/glue/tester.tcl
do_execsql_test json101-14.110 {
  SELECT fullkey FROM json_each('123.56');
} {$}
finish_test
