source $::suite_root/glue/tester.tcl
do_execsql_test instr-1.62 {
  SELECT coalesce(instr(NULL,NULL), 999);
} {999}
finish_test
