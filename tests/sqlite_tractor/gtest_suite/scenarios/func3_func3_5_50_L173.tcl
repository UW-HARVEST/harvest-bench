source $::suite_root/glue/tester.tcl
do_execsql_test func3-5.50 {
  SELECT likely(9223372036854775807);
} {9223372036854775807}
finish_test
