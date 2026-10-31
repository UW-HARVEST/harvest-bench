source $::suite_root/glue/tester.tcl
do_execsql_test json101-14.150 {
  SELECT fullkey FROM json_tree('123.56');
} {$}
finish_test
