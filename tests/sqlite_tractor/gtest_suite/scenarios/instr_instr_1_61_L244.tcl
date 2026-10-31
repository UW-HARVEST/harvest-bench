source $::suite_root/glue/tester.tcl
do_execsql_test instr-1.61 {
  SELECT coalesce(instr('abc',NULL), 999);
} {999}
finish_test
