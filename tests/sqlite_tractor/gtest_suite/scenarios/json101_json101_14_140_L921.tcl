source $::suite_root/glue/tester.tcl
do_execsql_test json101-14.140 {
  SELECT fullkey FROM json_tree('123');
} {$}
finish_test
