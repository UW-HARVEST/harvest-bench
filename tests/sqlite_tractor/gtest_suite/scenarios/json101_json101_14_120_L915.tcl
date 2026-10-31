source $::suite_root/glue/tester.tcl
do_execsql_test json101-14.120 {
  SELECT fullkey FROM json_each('"hello"');
} {$}
finish_test
