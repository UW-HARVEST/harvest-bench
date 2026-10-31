source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-170 {
  SELECT log(100.0)
} {2.0}
finish_test
