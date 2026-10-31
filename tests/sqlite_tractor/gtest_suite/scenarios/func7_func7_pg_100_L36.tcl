source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-100 {
  SELECT abs(-17.4)
} {17.4}
finish_test
