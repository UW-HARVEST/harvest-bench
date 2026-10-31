source $::suite_root/glue/tester.tcl
do_execsql_test func3-5.4 {
  SELECT likelihood(NULL, 0.5);
} {{}}
finish_test
