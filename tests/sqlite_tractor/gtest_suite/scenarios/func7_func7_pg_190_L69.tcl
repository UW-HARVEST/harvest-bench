source $::suite_root/glue/tester.tcl
do_execsql_test func7-pg-190 {
  SELECT log(2.0, 64.0)
} {6.0}
finish_test
