source $::suite_root/glue/tester.tcl
do_execsql_test func3-5.51 {
  SELECT likely(-9223372036854775808);
} {-9223372036854775808}
finish_test
