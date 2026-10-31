source $::suite_root/glue/tester.tcl
do_execsql_test 5.4 {
  SELECT ('{a: "abc'||char(0x5c,0x2028)||'xyz"}')->>'a';
} {abcxyz}
finish_test
