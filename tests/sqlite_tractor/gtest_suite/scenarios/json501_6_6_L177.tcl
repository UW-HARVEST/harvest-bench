source $::suite_root/glue/tester.tcl
do_execsql_test 6.6 {
  SELECT hex(('{a: "abc\0xyz"}')->>'a');
} {6162630078797A}
finish_test
