source $::suite_root/glue/tester.tcl
do_execsql_test json104-320 {
  SELECT json_patch('{"x":{"one":1}}','{"x":{"two":2},"x":"three"}');
} {{{"x":"three"}}}
finish_test
