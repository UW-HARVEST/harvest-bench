source $::suite_root/glue/tester.tcl
do_execsql_test json101-14.100 {
  SELECT fullkey FROM json_each('123');
} {$}
finish_test
