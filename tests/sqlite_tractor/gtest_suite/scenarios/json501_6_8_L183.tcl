source $::suite_root/glue/tester.tcl
do_execsql_test 6.8 {
  SELECT '{a: "\x6a\x6A\x6b\x6B\x6c\x6C\x6d\x6D\x6e\x6E\x6f\x6F"}'->>'a';
} {jjkkllmmnnoo}
finish_test
