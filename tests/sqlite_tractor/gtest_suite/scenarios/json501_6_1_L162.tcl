source $::suite_root/glue/tester.tcl
do_execsql_test 6.1 {
  SELECT ('{a: "abc'||char(0x5c,0x27)||'xyz"}')->>'a';
} {abc'xyz}
finish_test
