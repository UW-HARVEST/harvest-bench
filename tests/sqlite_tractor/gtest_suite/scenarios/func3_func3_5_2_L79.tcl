source $::suite_root/glue/tester.tcl
do_execsql_test func3-5.2 {
  SELECT likelihood(-9223372036854775808, 0.5);
} {-9223372036854775808}
finish_test
