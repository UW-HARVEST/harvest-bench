source $::suite_root/glue/tester.tcl
do_execsql_test 6.3 {
  SELECT ('{a: "abc'||char(0x5c,0x5c)||'xyz"}')->>'a';
} {{abc\xyz}}
finish_test
