source $::suite_root/glue/tester.tcl
do_execsql_test func7-210 {
  SELECT ln(-5), log(-5,100.0);
} {{} {}}
finish_test
