source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-160 {
  SELECT round(ln(2.0),7)
} {0.6931472}
finish_test
