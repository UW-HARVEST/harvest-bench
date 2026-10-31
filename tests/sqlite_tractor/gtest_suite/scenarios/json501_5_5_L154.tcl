source $::suite_root/glue/tester.tcl
do_execsql_test 5.5 {
  SELECT ('{a: "abc'||char(0x5c,0x2029)||'xyz"}')->>'a';
} {abcxyz}
finish_test
