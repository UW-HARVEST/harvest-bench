source $::suite_root/glue/tester.tcl
do_execsql_test func9-100 {
  SELECT concat('abc',123,null,'xyz');
} {abc123xyz}
finish_test
