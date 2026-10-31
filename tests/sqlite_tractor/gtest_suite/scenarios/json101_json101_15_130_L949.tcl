source $::suite_root/glue/tester.tcl
do_execsql_test json101-15.130 {
  SELECT xyz.* FROM (JSON_EACH('{"a":1, "b":2}')) AS xyz;
} {a 1 integer 1 1 {} {$.a} {$} b 2 integer 2 5 {} {$.b} {$}}
finish_test
