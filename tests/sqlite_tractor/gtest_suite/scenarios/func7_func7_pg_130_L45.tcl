source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-130 {
  SELECT round(exp(1.0),7)
} {2.7182818}
finish_test
