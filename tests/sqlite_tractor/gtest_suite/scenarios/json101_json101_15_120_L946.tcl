source $::suite_root/glue/tester.tcl
do_execsql_test json101-15.120 {
  SELECT * FROM (JSON_EACH('{"a":1, "b":2}'));
} {a 1 integer 1 1 {} {$.a} {$} b 2 integer 2 5 {} {$.b} {$}}
finish_test
