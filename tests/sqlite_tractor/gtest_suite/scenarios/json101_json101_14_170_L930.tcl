source $::suite_root/glue/tester.tcl
do_execsql_test json101-14.170 {
  SELECT fullkey FROM json_tree('null');
} {$}
finish_test
