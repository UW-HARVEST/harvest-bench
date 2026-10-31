source $::suite_root/glue/tester.tcl
do_execsql_test 6.2 {
  SELECT ('{a: "abc'||char(0x5c,0x22)||'xyz"}')->>'a';
} {abc"xyz}
finish_test
