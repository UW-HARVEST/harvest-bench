source $::suite_root/glue/tester.tcl
do_execsql_test instr-1.60 {
  SELECT coalesce(instr(NULL,'abc'), 999);
} {999}
finish_test
