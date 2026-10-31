source $::suite_root/glue/tester.tcl
do_execsql_test 5.3 {
  SELECT ('{a: "abc'||char(0x5c,0x0d,0x0a)||'xyz"}')->>'a';
} {abcxyz}
finish_test
