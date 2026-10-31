source $::suite_root/glue/tester.tcl
do_execsql_test func3-5.53 {
  SELECT likely(NULL);
} {{}}
finish_test
