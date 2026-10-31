source $::suite_root/glue/tester.tcl
do_execsql_test 6.7 {
  SELECT '{a: "abc\x35\x4f\x6Exyz"}'->>'a';
} {abc5Onxyz}
finish_test
