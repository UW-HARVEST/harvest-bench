source $::suite_root/glue/tester.tcl
do_execsql_test 6.5 {
  SELECT hex(('{a: "abc\f\n\r\t\vxyz"}')->>'a');
} {6162630C0A0D090B78797A}
finish_test
