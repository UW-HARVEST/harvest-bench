source $::suite_root/glue/tester.tcl
do_execsql_test json101-14.160 {
  SELECT fullkey FROM json_tree('"hello"');
} {$}
finish_test
