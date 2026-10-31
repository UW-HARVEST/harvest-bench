source $::suite_root/glue/tester.tcl
do_execsql_test 4.2 {
  SELECT '{b: 123, ''a'': ''ab\''cd''}'->>'a';
} {ab'cd}
finish_test
