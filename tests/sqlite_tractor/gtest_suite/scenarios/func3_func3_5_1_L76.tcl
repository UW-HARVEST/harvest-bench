source $::suite_root/glue/tester.tcl
do_execsql_test func3-5.1 {
  SELECT likelihood(9223372036854775807, 0.5);
} {9223372036854775807}
finish_test
