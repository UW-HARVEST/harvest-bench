source $::suite_root/glue/tester.tcl
do_execsql_test 6.4 {
  SELECT hex(('{a: "abc\bxyz"}')->>'a');
} {6162630878797A}
finish_test
